#include "grove/rhi/vulkan/vk_device.hpp"
#include "grove/rhi/vulkan/vk_context.hpp"
#include "vulkan/vulkan_profiles.hpp"
#include <map>
#include <set>
#include "grove/core/window.hpp"
#include <fstream>
#include <utility>
#include "grove/rhi/command_buffer.hpp"

namespace grove
{
	namespace
	{
		VpCapabilities capabilities_ = VK_NULL_HANDLE;
		constexpr VpProfileProperties profileProps_ = VKContext::GetVPProfileProperties();
	}

	VKDevice::VKDevice()
	{
	}

	VKDevice::~VKDevice()
	{
	}

	VKDevice::VKDevice(VKDevice&& other) noexcept
	{
		vkContext_ = std::exchange(other.vkContext_, nullptr);
		window_ = std::exchange(other.window_, nullptr);
		physicalDevice_ = std::move(other.physicalDevice_);
		device_ = std::move(other.device_);
		queueFamilyIndices_ = other.queueFamilyIndices_;
		queues_ = std::move(other.queues_);
		swapchain_ = std::move(other.swapchain_);
		graphicsPipeline_ = std::move(other.graphicsPipeline_);
		commandData_ = std::move(other.commandData_);
		semaphores_ = std::move(other.semaphores_);
		fences_ = std::move(other.fences_);
		perFrame_.currentFrame = other.perFrame_.currentFrame;
		other.perFrame_.currentFrame = 0;
	}

	VKDevice& VKDevice::operator=(VKDevice&& other) noexcept
	{
		if (this != &other)
		{
			vkContext_ = std::exchange(other.vkContext_, nullptr);
			window_ = std::exchange(other.window_, nullptr);
			physicalDevice_ = std::move(other.physicalDevice_);
			device_ = std::move(other.device_);
			queueFamilyIndices_ = other.queueFamilyIndices_;
			queues_ = std::move(other.queues_);
			swapchain_ = std::move(other.swapchain_);
			graphicsPipeline_ = std::move(other.graphicsPipeline_);
			commandData_ = std::move(other.commandData_);
			semaphores_ = std::move(other.semaphores_);
			fences_ = std::move(other.fences_);
			perFrame_.currentFrame = other.perFrame_.currentFrame;
			other.perFrame_.currentFrame = 0;
		}

		return *this;
	}

	Status VKDevice::BeginFrame()
	{
		vk::Result fenceResult = device_.waitForFences(*fences_.inFlightFences[perFrame_.currentFrame], vk::True, UINT64_MAX);
		GRV_ERR_IF_MSG(fenceResult != vk::Result::eSuccess, Failed, "vk.waitForInFlightFences.failed result={}", vk::to_string(fenceResult));
		device_.resetFences(*fences_.inFlightFences[perFrame_.currentFrame]);

		auto [result, imageIndex] = swapchain_.swapchain.acquireNextImage(UINT64_MAX, *semaphores_.imageAvailableSemaphores[perFrame_.currentFrame], nullptr);
		GRV_ERR_IF_MSG(result != vk::Result::eSuccess, Failed, "Failed to acquire next swapchain image result={}", vk::to_string(result));
		perFrame_.currentImageIndex = imageIndex;
		commandData_.commandBuffers[perFrame_.currentFrame].reset();

		return GRV_OK;
	}

	Status VKDevice::EndFrame()
	{
		// submit for drawing
		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::SubmitInfo submitInfo{};
		submitInfo
			.setWaitSemaphores(*semaphores_.imageAvailableSemaphores[perFrame_.currentFrame])
			.setWaitDstStageMask(waitDestinationStageMask)
			.setCommandBuffers(*commandData_.commandBuffers[perFrame_.currentFrame])
			.setSignalSemaphores(*semaphores_.renderFinishedSemaphores[perFrame_.currentImageIndex]);
		queues_.graphicsQueue.submit(submitInfo, *fences_.inFlightFences[perFrame_.currentFrame]);

		// submit for presentation
		vk::PresentInfoKHR presentInfoKHR{};
		presentInfoKHR
			.setWaitSemaphores(*semaphores_.renderFinishedSemaphores[perFrame_.currentImageIndex])
			.setSwapchains(*swapchain_.swapchain)
			.setImageIndices(perFrame_.currentImageIndex);
		vk::Result result = queues_.presentQueue.presentKHR(presentInfoKHR);
		GRV_ERR_IF_MSG(result != vk::Result::eSuccess, Failed, "Failed to Present result={}", vk::to_string(result));

		perFrame_.currentFrame = (perFrame_.currentFrame + 1) % perFrame_.MAX_FRAMES_IN_FLIGHT;
		return GRV_OK;
	}

	Result<VKDevice> VKDevice::Create(const VKDeviceCreateInfo& createInfo)
	{
		VKDevice device;
		device.vkContext_ = createInfo.vkContext;
		device.window_ = createInfo.window;

		GRV_TRY(device.PickPhysicalDevice());
		GRV_TRY(device.CreateLogicalDevice());
		GRV_TRY(device.CreateSwapChain());
		GRV_TRY(device.CreateImageViews());
		GRV_TRY(device.CreateGraphicsPipeline());
		GRV_TRY(device.CreateCommandPool());
		GRV_TRY(device.CreateCommandBuffers());
		GRV_TRY(device.CreateSyncObjects());

		return device;
	}

	Status VKDevice::PickPhysicalDevice()
	{
		const vk::raii::Instance& instance = vkContext_->GetInstance();

		std::vector<vk::raii::PhysicalDevice> physicalDevices = vkContext_->GetInstance().enumeratePhysicalDevices().value();
		GRV_ERR_IF_MSG(physicalDevices.empty(), CantCreate,
			"No physical devices found");

		using deviceScore = u64;
		std::multimap<deviceScore, vk::raii::PhysicalDevice> deviceScores;
		for (const auto& physicalDevice : physicalDevices)
		{
			u64 deviceScore = RankPhysicalDevice(physicalDevice);
			deviceScores.insert(std::make_pair(deviceScore, physicalDevice));
		}

		u64 highestScore = deviceScores.rbegin()->first;
		GRV_ERR_IF(highestScore <= 0, CantCreate);

		auto& bestCandidate = deviceScores.rbegin()->second;
		physicalDevice_ = std::move(bestCandidate);

		GRV_TRY(GetQueueFamilyIndices());

		vk::PhysicalDeviceProperties physicalDeviceProps = physicalDevice_.getProperties();

		GRV_LOG_INFO("Selected GPU name='{}'", std::string_view{ physicalDeviceProps.deviceName });
		return GRV_OK;
	}

	Status VKDevice::CreateLogicalDevice()
	{
		using queueIndex = u32;
		std::set<queueIndex> uniqueQueueFamilies
		{
			queueFamilyIndices_.graphics,
			queueFamilyIndices_.present
		};

		f32 queuePriority{ 1.0f };
		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		for (queueIndex queueFamily : uniqueQueueFamilies)
		{
			queueCreateInfos.emplace_back(
				vk::DeviceQueueCreateInfo
				{
					.queueFamilyIndex { queueFamily },
					.queueCount       { 1 },
					.pQueuePriorities { &queuePriority }
				}
			);
		}

		const std::array requiredExtensions
		{
			vk::KHRSwapchainExtensionName
		};

		vk::DeviceCreateInfo vkDeviceCreateInfo{};
		vkDeviceCreateInfo
			.setQueueCreateInfos(queueCreateInfos)
			.setPEnabledExtensionNames(requiredExtensions);

		VpDeviceCreateInfo vpDeviceCreateInfo
		{
			.pCreateInfo = &*vkDeviceCreateInfo,
			.enabledFullProfileCount = 1,
			.pEnabledFullProfiles = &profileProps_,
		};

		VkDevice device = VK_NULL_HANDLE;
		vk::Result result = vk::Result(vpCreateDevice(capabilities_, *physicalDevice_, &vpDeviceCreateInfo, nullptr, &device));
		GRV_ERR_IF_MSG(result != vk::Result::eSuccess, CantCreate, "vpCreateDevice failed. result={}", vk::to_string(result));

		device_ = vk::raii::Device(physicalDevice_, device);
		queues_.graphicsQueue = device_.getQueue(queueFamilyIndices_.graphics, 0);
		queues_.presentQueue = device_.getQueue(queueFamilyIndices_.present, 0);

		return GRV_OK;
	}

	Status VKDevice::CreateSwapChain()
	{
		auto& surface = vkContext_->GetSurface();

		auto capabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface);
		GRV_ERR_IF(!capabilities, CantCreate);

		auto formats = physicalDevice_.getSurfaceFormatsKHR(surface);
		GRV_ERR_IF(!formats, CantCreate);

		auto presentModes = physicalDevice_.getSurfacePresentModesKHR(surface);
		GRV_ERR_IF(!presentModes, CantCreate);

		vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(*formats);
		vk::PresentModeKHR presentMode = ChooseSwapPresentMode(*presentModes, vk::PresentModeKHR::eImmediate);
		vk::Extent2D extent = ChooseSwapExtent(*capabilities);

		grove::u32 imageCount{ capabilities->minImageCount + 1 };
		if (capabilities->maxImageCount > 0 && imageCount > capabilities->maxImageCount)
		{
			imageCount = capabilities->maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapChainInfo;
		swapChainInfo
			.setFlags(vk::SwapchainCreateFlagsKHR())
			.setSurface(surface)
			.setMinImageCount(imageCount)
			.setImageFormat(surfaceFormat.format)
			.setImageColorSpace(surfaceFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

		if (queueFamilyIndices_.graphics != queueFamilyIndices_.present)
		{
			grove::u32 queueFamilyIndices[]
			{
				queueFamilyIndices_.graphics,
				queueFamilyIndices_.present
			};

			swapChainInfo
				.setImageSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndexCount(2)
				.setQueueFamilyIndices(queueFamilyIndices);
		}
		else
		{
			swapChainInfo
				.setImageSharingMode(vk::SharingMode::eExclusive)
				.setQueueFamilyIndexCount(0)      // optional
				.setPQueueFamilyIndices(nullptr); // optional
		}

		swapChainInfo
			.setPreTransform(capabilities->currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(presentMode)
			.setClipped(vk::True)
			.setOldSwapchain(VK_NULL_HANDLE);

		auto swapchain = device_.createSwapchainKHR(swapChainInfo);
		GRV_ERR_IF(!swapchain, CantCreate);

		swapchain_.swapchain = std::move(*swapchain);
		swapchain_.surfaceFormat = std::move(surfaceFormat);
		swapchain_.extent = std::move(extent);

		auto swapchainImages = swapchain_.swapchain.getImages();
		GRV_ERR_IF_MSG(!swapchainImages, CantCreate, "Failed to get swapchain images result={}", vk::to_string(swapchainImages.error()));
		swapchain_.images = std::move(*swapchainImages);

		GRV_LOG_INFO("event=vk.swapChain.created format={} extent={}x{} imageCount={}",
			vk::to_string(surfaceFormat.format),
			extent.width,
			extent.height,
			swapchain_.images.size());
		return GRV_OK;
	}

	Status VKDevice::CreateImageViews()
	{
		swapchain_.imageViews.clear();

		vk::ComponentMapping componentMapping{};
		componentMapping
			// don't rearrange color channels
			.setR(vk::ComponentSwizzle::eIdentity)
			.setG(vk::ComponentSwizzle::eIdentity)
			.setB(vk::ComponentSwizzle::eIdentity)
			.setA(vk::ComponentSwizzle::eIdentity);

		// which part of the image this view can access
		vk::ImageSubresourceRange subresourceRange{};
		subresourceRange
			.setAspectMask(vk::ImageAspectFlagBits::eColor)

			// expose only mip level 0 (full-resolution mip)
			.setBaseMipLevel(0)
			.setLevelCount(1)

			// expose only array layer 0 (non-array image)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		vk::ImageViewCreateInfo imgViewInfo{};
		imgViewInfo
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(swapchain_.surfaceFormat.format)
			.setComponents(componentMapping)
			.setSubresourceRange(subresourceRange);

		swapchain_.imageViews.reserve(swapchain_.images.size());
		for (const vk::Image& image : swapchain_.images)
		{
			imgViewInfo.setImage(image);

			auto imageView = device_.createImageView(imgViewInfo);
			GRV_ERR_IF_MSG(!imageView, CantCreate, "createImageView failed result={}", vk::to_string(imageView.error()));

			swapchain_.imageViews.emplace_back(std::move(*imageView));
		}

		return GRV_OK;
	}

	Status VKDevice::CreateGraphicsPipeline()
	{
		std::vector<char> shaderCode;
		GRV_TRY_ASSIGN(shaderCode, ReadFile("assets/shaders/slang.spv"));

		vk::raii::ShaderModule shaderModule{ nullptr };
		GRV_TRY_ASSIGN(shaderModule, CreateShaderModule(shaderCode));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(shaderModule)
			.setPName("vertMain");

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(shaderModule)
			.setPName("fragMain");

		vk::PipelineShaderStageCreateInfo shaderStages[]
		{
			vertShaderStageInfo,
			fragShaderStageInfo
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

		vk::PipelineInputAssemblyStateCreateInfo assemblyInputInfo{};
		assemblyInputInfo
			.setTopology(vk::PrimitiveTopology::eTriangleList);

		vk::PipelineViewportStateCreateInfo viewportState{};
		viewportState
			.setViewportCount(1)
			.setScissorCount(1);

		vk::PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer
			.setDepthClampEnable(vk::False)
			.setRasterizerDiscardEnable(vk::False)
			.setPolygonMode(vk::PolygonMode::eFill)
			//.setCullMode(vk::CullModeFlagBits::eBack)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eClockwise)
			.setDepthBiasEnable(vk::False)
			.setDepthBiasSlopeFactor(1.0f)
			.setLineWidth(1.0f);

		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling
			.setRasterizationSamples(vk::SampleCountFlagBits::e1)
			.setSampleShadingEnable(vk::False);

		vk::PipelineColorBlendAttachmentState colorBlendAttachement{};
		colorBlendAttachement
			.setBlendEnable(vk::True)

			// allowed channels to overwrite
			.setColorWriteMask(vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA)

			// alpha blend color values
			// out = (srcColor * srcAlpha) + (dstColor * (1 - srcAlpha))
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setColorBlendOp(vk::BlendOp::eAdd)

			// overwrite alpha value
			// outAlpha = (srcAlpha * 1) + (dstAlpha * 0)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
			.setAlphaBlendOp(vk::BlendOp::eAdd);

		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending
			.setLogicOpEnable(vk::False)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachement);

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo
			.setSetLayoutCount(0)
			.setPushConstantRangeCount(0);

		std::vector<vk::DynamicState> dynamicStates
		{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.setDynamicStates(dynamicStates);

		auto pipelineLayout = device_.createPipelineLayout(pipelineLayoutInfo);
		GRV_ERR_IF_MSG(!pipelineLayout, CantCreate, "vk.createPipelineLayout.failed result={}", vk::to_string(pipelineLayout.error()));

		vk::StructureChain<
			vk::GraphicsPipelineCreateInfo,
			vk::PipelineRenderingCreateInfo
		> pipelineInfoChain{};

		auto& graphicsPipelineInfo = pipelineInfoChain.get<vk::GraphicsPipelineCreateInfo>();
		graphicsPipelineInfo
			.setStages(shaderStages)
			.setPVertexInputState(&vertexInputInfo)
			.setPInputAssemblyState(&assemblyInputInfo)
			.setPViewportState(&viewportState)
			.setPRasterizationState(&rasterizer)
			.setPMultisampleState(&multisampling)
			.setPColorBlendState(&colorBlending)
			.setPDynamicState(&dynamicState)
			.setLayout(*pipelineLayout)
			.setRenderPass(nullptr);

		auto& pipelineRenderingInfo = pipelineInfoChain.get<vk::PipelineRenderingCreateInfo>();
		pipelineRenderingInfo
			.setColorAttachmentFormats(swapchain_.surfaceFormat.format);

		auto graphicsPipeline = device_.createGraphicsPipeline(nullptr, graphicsPipelineInfo);
		GRV_ERR_IF_MSG(!graphicsPipeline, CantCreate, "vk.createGraphicsPipeline.failed result={}", vk::to_string(graphicsPipeline.error()));

		graphicsPipeline_.pipeline = std::move(*graphicsPipeline);
		GRV_LOG_INFO("event=vk.graphicsPipeline.created");
		return GRV_OK;
	}

	Status VKDevice::CreateCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{};
		poolInfo
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(queueFamilyIndices_.graphics);

		auto commandPool = device_.createCommandPool(poolInfo);
		GRV_ERR_IF_MSG(!commandPool, CantCreate, "vk.createCommandPool.failed result={}", vk::to_string(commandPool.error()));

		commandData_.commandPool = std::move(*commandPool);
		GRV_LOG_INFO("event=vk.commandPool.created graphicsFamilyIndex={}", poolInfo.queueFamilyIndex);

		return GRV_OK;
	}

	Status VKDevice::CreateCommandBuffers()
	{
		commandData_.commandBuffers.clear();

		vk::CommandBufferAllocateInfo allocInfo{};
		allocInfo
			.setCommandPool(commandData_.commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(perFrame_.MAX_FRAMES_IN_FLIGHT);

		auto commandBuffers = device_.allocateCommandBuffers(allocInfo);
		GRV_ERR_IF_MSG(!commandBuffers, CantCreate, "vk.createCommandBuffers.failed result={}", vk::to_string(commandBuffers.error()));

		commandData_.commandBuffers = std::move(*commandBuffers);

		GRV_LOG_INFO("event=vk.commandBuffers.created level={} bufferCount={}", vk::to_string(vk::CommandBufferLevel::ePrimary), 1);
		return GRV_OK;
	}

	Status VKDevice::CreateSyncObjects()
	{
		GRV_ASSERT(
			semaphores_.imageAvailableSemaphores.empty() &&
			semaphores_.renderFinishedSemaphores.empty() &&
			fences_.inFlightFences.empty()
		);

		vk::SemaphoreCreateInfo semaphoreInfo{};

		for (size_t i = 0; i < swapchain_.images.size(); ++i)
		{
			auto renderFinishedSemaphore = device_.createSemaphore(semaphoreInfo);
			GRV_ERR_IF_MSG(!renderFinishedSemaphore, CantCreate, "vk.createRenderFinishedSemaphore.failed result={}", vk::to_string(renderFinishedSemaphore.error()));
			semaphores_.renderFinishedSemaphores.emplace_back(std::move(*renderFinishedSemaphore));
		}

		for (size_t i = 0; i < perFrame_.MAX_FRAMES_IN_FLIGHT; ++i)
		{
			auto imageAvailableSemaphore = device_.createSemaphore(semaphoreInfo);
			GRV_ERR_IF_MSG(!imageAvailableSemaphore, CantCreate, "vk.createImageAvailableSemaphore.failed result={}", vk::to_string(imageAvailableSemaphore.error()));
			semaphores_.imageAvailableSemaphores.emplace_back(std::move(*imageAvailableSemaphore));

			vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };
			auto fence = device_.createFence(fenceInfo);
			GRV_ERR_IF_MSG(!fence, CantCreate, "vk.createFence.failed result={}", vk::to_string(fence.error()));

			fences_.inFlightFences.emplace_back(std::move(*fence));
		}

		GRV_LOG_INFO("event=vk.syncObjects.created semaphoreCount={} fenceCount={}",
			semaphores_.imageAvailableSemaphores.size() + semaphores_.renderFinishedSemaphores.size(),
			fences_.inFlightFences.size());
		return GRV_OK;
	}

	Status VKDevice::RecordCommandBuffer(u32 imageIndex)
	{
		auto& commandBuffer = commandData_.commandBuffers[perFrame_.currentFrame];
		commandBuffer.begin({});

		//TransitionImageLayout(
		//	imageIndex,
		//	vk::ImageLayout::eUndefined,
		//	vk::ImageLayout::eColorAttachmentOptimal,
		//	{},
		//	vk::AccessFlagBits2::eColorAttachmentWrite,
		//	vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		//	vk::PipelineStageFlagBits2::eColorAttachmentOutput
		//);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo{};
		attachmentInfo
			.setImageView(swapchain_.imageViews[imageIndex])
			.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setClearValue(clearColor);

		vk::RenderingInfo renderingInfo{};
		renderingInfo
			.setRenderArea(
				{
					.offset { 0, 0 },
					.extent { swapchain_.extent }
				}
			)
			.setLayerCount(1)
			.setColorAttachments(attachmentInfo);

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline_.pipeline);

		vk::Viewport viewport
		{
			.x        { 0.0f },
			.y        { 0.0f },
			.width    { static_cast<grove::f32>(swapchain_.extent.width) },
			.height   { static_cast<grove::f32>(swapchain_.extent.height) },
			.minDepth { 0.0f },
			.maxDepth { 1.0f }
		};

		commandBuffer.setViewport(0, viewport);
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_.extent));
		commandBuffer.draw(3, 1, 0, 0);

		commandBuffer.endRendering();

		//TransitionImageLayout(
		//	imageIndex,
		//	vk::ImageLayout::eColorAttachmentOptimal,
		//	vk::ImageLayout::ePresentSrcKHR,
		//	vk::AccessFlagBits2::eColorAttachmentWrite,
		//	{},
		//	vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		//	vk::PipelineStageFlagBits2::eBottomOfPipe
		//);

		commandBuffer.end();
		return GRV_OK;
	}

	Status VKDevice::RecordCommands(const CommandBuffer& commandBuffer)
	{
		auto& cmd = commandData_.commandBuffers[perFrame_.currentFrame];
		cmd.begin({});

		for (const auto& command : commandBuffer.GetCommands())
		{
			switch (command.type)
			{
				using enum CommandType;
			case BeginRendering:
			{
				const RGBAColor& clearColor = command.payload.beginRendering.clearColor;

				vk::RenderingAttachmentInfo attachmentInfo{};
				attachmentInfo
					.setImageView(swapchain_.imageViews[perFrame_.currentImageIndex])
					.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setLoadOp(vk::AttachmentLoadOp::eClear)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setClearValue(vk::ClearColorValue(clearColor.red, clearColor.green, clearColor.blue, clearColor.alpha));

				vk::RenderingInfo renderingInfo{};
				renderingInfo
					.setRenderArea(
						{
							.offset { 0, 0 },
							.extent { swapchain_.extent }
						}
					)
					.setLayerCount(1)
				.setColorAttachments(attachmentInfo);

				cmd.beginRendering(renderingInfo);
				break;
			}
			case EndRendering:
			{
				cmd.endRendering();
				break;
			}
			case Draw:
			{
				const DrawCommand& draw = command.payload.draw;

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline_.pipeline);

				vk::Viewport viewport
				{
					.x        { 0.0f },
					.y        { 0.0f },
					.width    { static_cast<grove::f32>(swapchain_.extent.width) },
					.height   { static_cast<grove::f32>(swapchain_.extent.height) },
					.minDepth { 0.0f },
					.maxDepth { 1.0f }
				};

				cmd.setViewport(0, viewport);
				cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_.extent));
				cmd.draw(draw.vertexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
				break;
			}
			case Barrier:
			{
				TransitionInfo transitionInfo = GetTransitionInfo(command.payload.barrier);
				GRV_TRY(TransitionImageLayout(transitionInfo));
				break;
			}
			default:
				break;
			}
		}


		cmd.end();
		return GRV_OK;
	}

	Status VKDevice::TransitionImageLayout(const TransitionInfo& transitionInfo)
	{
		vk::ImageMemoryBarrier2 barrier{};
		barrier
			.setSrcAccessMask(transitionInfo.srcAccessMask)
			.setSrcStageMask(transitionInfo.srcStageMask)
			.setDstAccessMask(transitionInfo.dstAccessMask)
			.setDstStageMask(transitionInfo.dstStageMask)
			.setOldLayout(transitionInfo.oldLayout)
			.setNewLayout(transitionInfo.newLayout)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(swapchain_.images[transitionInfo.imageIndex])
			.setSubresourceRange(
				{
					.aspectMask     { vk::ImageAspectFlagBits::eColor },
					.baseMipLevel   { 0 },
					.levelCount     { 1 },
					.baseArrayLayer { 0 },
					.layerCount     { 1 }
				}
			);

		vk::DependencyInfo dependencyInfo{};
		dependencyInfo
			.setDependencyFlags({})
			.setImageMemoryBarrierCount(1)
			.setImageMemoryBarriers(barrier);

		commandData_.commandBuffers[perFrame_.currentFrame].pipelineBarrier2(dependencyInfo);

		return GRV_OK;
	}

	VKDevice::TransitionInfo VKDevice::GetTransitionInfo(const BarrierCommand& barrierCommand)
	{
		if (barrierCommand.before == ResourceState::Undefined && barrierCommand.after == ResourceState::RenderTarget)
		{
			return
			{
			perFrame_.currentImageIndex,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal,
				{},
				vk::AccessFlagBits2::eColorAttachmentWrite,
				vk::PipelineStageFlagBits2::eTopOfPipe,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput
			};
		}

		if (barrierCommand.before == ResourceState::RenderTarget && barrierCommand.after == ResourceState::Present)
		{
			return
			{
				perFrame_.currentImageIndex,
				vk::ImageLayout::eColorAttachmentOptimal,
				vk::ImageLayout::ePresentSrcKHR,
				vk::AccessFlagBits2::eColorAttachmentWrite,
				{},
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::PipelineStageFlagBits2::eBottomOfPipe
			};
		}

		if (barrierCommand.before == ResourceState::Present && barrierCommand.after == ResourceState::RenderTarget)
		{
			return
			{
				perFrame_.currentImageIndex,
				vk::ImageLayout::ePresentSrcKHR,
				vk::ImageLayout::eColorAttachmentOptimal,
				{},
				vk::AccessFlagBits2::eColorAttachmentWrite,
				vk::PipelineStageFlagBits2::eTopOfPipe,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput
			};
		}

		GRV_ASSERT(false);
		return {};
	}

	Result<std::vector<char>> VKDevice::ReadFile(const std::string& filename) const
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		GRV_ERR_IF_MSG(!file.is_open(), FileCantOpen, "{} could not be opened for reading", filename);

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();

		return buffer;
	}

	[[nodiscard]] Result<vk::raii::ShaderModule> VKDevice::CreateShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo shaderModuleInfo
		{
			.codeSize { code.size() * sizeof(char) },
			.pCode    { reinterpret_cast<const grove::u32*>(code.data()) }
		};

		auto shaderModule = device_.createShaderModule(shaderModuleInfo);
		GRV_ERR_IF_MSG(!shaderModule, CantCreate, "Couldn't create shader module result={}", vk::to_string(shaderModule.error()));

		return std::move(*shaderModule);
	}

	vk::SurfaceFormatKHR VKDevice::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const
	{
		for (const vk::SurfaceFormatKHR& format : formats)
		{
			// pick srgb if available
			if (format.format == vk::Format::eB8G8R8A8Srgb &&
				format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				return format;
			}
		}

		return formats.front();
	}

	vk::PresentModeKHR VKDevice::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes, vk::PresentModeKHR requestedMode) const
	{
		for (vk::PresentModeKHR presentMode : presentModes)
		{
			if (presentMode == requestedMode)
			{
				return presentMode;
			}
		}

		// fifo guaranteed by spec
		GRV_LOG_WARN(
			"event=vk.swapChain.presentModeUnavailable requested=\"{}\" fallback=\"FIFO\"",
			vk::to_string(requestedMode));

		return vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D VKDevice::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<grove::u32>::max())
		{
			return capabilities.currentExtent;
		}

		return
		{
			std::clamp<grove::u32>(window_->GetWidth(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<grove::u32>(window_->GetHeight(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	}

	Status VKDevice::GetQueueFamilyIndices()
	{
		QueueFamilyIndices queueIndices{};

		std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice_.getQueueFamilyProperties();

		bool graphicsIndexFound{ false };
		bool presentIndexFound{ false };
		for (grove::u32 i{}; i < queueFamilies.size(); ++i)
		{
			if (graphicsIndexFound && presentIndexFound)
			{
				break;
			}

			if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0))
			{
				queueIndices.graphics = i;
				graphicsIndexFound = true;
			}

			auto result = physicalDevice_.getSurfaceSupportKHR(i, vkContext_->GetSurface());
			GRV_ERR_IF_MSG(!result, CantCreate, "getSurfaceSupportKHR failed. Cannot determine if presentation is supported.");

			vk::Bool32 presentSupported = *result;
			if (presentSupported == vk::True)
			{
				queueIndices.present = i;
				presentIndexFound = true;
			}
		}

		GRV_ERR_IF_MSG(!graphicsIndexFound, CantCreate, "Couldn't find a graphics index for physical device.");
		GRV_ERR_IF_MSG(!presentIndexFound, CantCreate, "Couldn't find a present index for physical device.");

		queueFamilyIndices_ = queueIndices;
		return GRV_OK;
	}

	u64 VKDevice::RankPhysicalDevice(const vk::raii::PhysicalDevice& physicalDevice) const
	{
		const vk::raii::Instance& instance = vkContext_->GetInstance();

		VpCapabilitiesCreateInfo vpCapsCreateInfo
		{
			.flags = VP_PROFILE_CREATE_STATIC_BIT,
			.apiVersion = VK_API_VERSION_1_1,
			.pVulkanFunctions = nullptr
		};

		vpCreateCapabilities(&vpCapsCreateInfo, nullptr, &capabilities_);

		vk::Bool32 profileSupported = vk::False;
		vk::Result result = vk::Result(vpGetPhysicalDeviceProfileSupport(
			capabilities_,
			*instance,
			*physicalDevice,
			&profileProps_,
			&profileSupported));

		if (result != vk::Result::eSuccess || profileSupported != vk::True)
		{
			return 0; // Disqualified
		}

		const vk::PhysicalDeviceProperties deviceProps = physicalDevice.getProperties();

		if (deviceProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			return 1000;
		}

		return 0;
	}
}
