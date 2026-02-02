#include <array>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include <fstream>
#include <filesystem>
#include <ranges>
#include <bit>
#include <cmath>

#include "grove/core/math.hpp"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>

#include "grove/core/logging.hpp"
#include "grove/core/memory.hpp"
#include "grove/core/window.hpp"
#include "grove/core/assert.hpp"
#include "grove/core/grove_engine.hpp"
#include "grove/core/typedefs.hpp"
#include "grove/platform/win32_window.hpp"

#define KiB(x) ((x) >> 10)
#define MiB(x) ((x) >> 20)
#define GiB(x) ((x) >> 30)

#define VK_CHECK(result, failureEvent) \
do { \
	if (!result)\
	{\
		GRV_LOG_ERROR(GRV_CHANNEL(System), "event={} result={}", failureEvent, vk::to_string(result.error()));\
		return result.error();\
	}\
} while(0)

enum class VSyncSetting : grove::u8
{
	OFF = 0,
	ON,
	ADAPTIVE,
	TRIPLE_BUFFERING
};

constexpr std::string_view ToString(VSyncSetting vsync)
{
	switch (vsync)
	{
	using enum VSyncSetting;
	case OFF:              return "OFF";
	case ON:               return "ON";
	case ADAPTIVE:         return "ADAPTIVE";
	case TRIPLE_BUFFERING: return "TRIPLE_BUFFERING";
	default:               GRV_ASSERT(false, "Invalid VSyncSetting"); return "Unknown";
	}
}

constexpr vk::PresentModeKHR ToPresentMode(VSyncSetting setting)
{
	switch (setting)
	{
	using enum VSyncSetting;
	case OFF:              return vk::PresentModeKHR::eImmediate;
	case ON:               return vk::PresentModeKHR::eFifo;
	case ADAPTIVE:         return vk::PresentModeKHR::eFifoRelaxed;
	case TRIPLE_BUFFERING: return vk::PresentModeKHR::eMailbox;
	default:               GRV_ASSERT(false, "Invalid VSyncSetting"); return vk::PresentModeKHR::eFifo;
	}
};

struct GraphicsSettings
{
	VSyncSetting vsyncSetting;
};

namespace
{
	constexpr std::array kValidationLayers
	{ 
		"VK_LAYER_KHRONOS_validation" 
	};

	constexpr std::array kDeviceExtensions
	{
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRCreateRenderpass2ExtensionName
	};

	constexpr std::array kRequiredInstanceExtensions
	{
		vk::KHRSurfaceExtensionName,
		vk::KHRWin32SurfaceExtensionName
	};

	template<typename T, typename GetNameFunc>
	bool ValidateSupport(
		std::span<const T> available,
		std::span<const char* const> required,
		const char* typeName,
		GetNameFunc getName
	)
	{
		std::unordered_set<std::string_view> requiredSet(required.begin(), required.end());

		for (const auto& item : available)
		{
			requiredSet.erase(getName(item));
		}

		if (!requiredSet.empty())
		{
			for (const auto& missing : requiredSet)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.validation.missing type={} name={}", typeName, missing);
			}
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.validation.failed type={} missingCount={}", typeName, requiredSet.size());
			return false;
		}

		return true;
	}

	bool AreValidationLayersSupported(vk::raii::Context& context, std::span<const char* const> layers)
	{
		auto availableLayers = context.enumerateInstanceLayerProperties();

		std::unordered_set<std::string_view> requiredSet(kValidationLayers.begin(), kValidationLayers.end());

		for (const auto& item : availableLayers)
		{
			requiredSet.erase(item.layerName);
		}

		if (!requiredSet.empty())
		{
			for (const auto& missing : requiredSet)
			{
				GRV_LOG_WARN(GRV_CHANNEL(System), "event=vk.validationLayer.missing name={}", missing);
			}

			return false;
		}

		return true;
	}

	bool AreInstanceExtensionsSupported(vk::raii::Context& context, std::span<const char* const> extensions)
	{
		auto availableExtensions = context.enumerateInstanceExtensionProperties();

		return ValidateSupport<vk::ExtensionProperties>(
			std::span{availableExtensions},
			extensions,
			"instance extension",
			[](const vk::ExtensionProperties& prop) -> std::string_view { return prop.extensionName; }
		);
	}

	bool AreDeviceExtensionsSupported(vk::raii::PhysicalDevice& device, std::span<const char* const> extensions)
	{
		auto availableExtensions = device.enumerateDeviceExtensionProperties();

		return ValidateSupport<vk::ExtensionProperties>(
			std::span{availableExtensions},
			extensions,
			"device extension",
			[](const vk::ExtensionProperties& prop) -> std::string_view { return prop.extensionName; }
		);
	}
	
	std::expected<std::vector<char>, std::string> ReadFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			return std::unexpected(std::format("{} could not be opened for reading.", filename));
		}

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();

		return buffer;
	}
}

struct QueueFamilyIndices
{
	std::optional<grove::u32> graphicsFamily { std::nullopt };
	std::optional<grove::u32> presentFamily  { std::nullopt };

	bool IsComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

struct VulkanContext
{
	vk::raii::Context                context;

	// instance level
	vk::raii::Instance               instance                 { nullptr };
	vk::raii::DebugUtilsMessengerEXT debugMessenger           { nullptr };
	vk::raii::SurfaceKHR             surface                  { nullptr };

	// device level
	vk::raii::PhysicalDevice         physicalDevice           { nullptr };
	vk::raii::Device                 device                   { nullptr };
	QueueFamilyIndices               queueFamilyIndices;

	// swap chain & queues
	vk::raii::SwapchainKHR           swapChain                { nullptr };
	vk::raii::Queue                  graphicsQueue            { nullptr };
	vk::raii::Queue                  presentQueue             { nullptr };
	std::vector<vk::Image>           swapChainImages;
	std::vector<vk::raii::ImageView> swapChainImageViews;
	vk::SurfaceFormatKHR             swapChainSurfaceFormat;
	vk::Extent2D                     swapChainExtent;

	// pipeline
	vk::raii::PipelineLayout         pipelineLayout           { nullptr };
	vk::raii::Pipeline               graphicsPipeline         { nullptr };

	// Commands
	vk::raii::CommandPool            commandPool              { nullptr };
	vk::raii::CommandBuffer          commandBuffer            { nullptr };

	// Sync
	vk::raii::Semaphore              presentCompleteSemaphore { nullptr };
	vk::raii::Semaphore              renderFinishedSemaphore  { nullptr };
	vk::raii::Fence                  drawFence                { nullptr };

	#if defined(NDEBUG)
	bool                             enableValidationLayers   { false };
	#else
	bool                             enableValidationLayers   { true };
	#endif
};

class HelloTriangleApplication
{
public:

	bool Run()
	{
		InitEngine();
		InitWindow();

		if (!InitVulkan())
		{
			Cleanup();
			return false;
		}

		MainLoop();
		Cleanup();

		return true;
	}

private:
	void InitEngine()
	{
		engine_.Init();

		GRV_SET_LOG_LEVEL(Trace);
	}

	void InitWindow()
	{
		using namespace grove;

		WindowCreateInfo windowCreateInfo 
		{
			.title                { "GroveEngine" },
			.width                { 800 },
			.height               { 600 },
			.enableDebugConsole   { true }
		};

		window_ = grove::Window::Create(windowCreateInfo);
	}

#define VK_INIT_STEP(expr) \
do { \
	if (auto r = (expr); r != vk::Result::eSuccess) {\
		return false; \
	} \
} while (0)

	bool InitVulkan()
	{
		VK_INIT_STEP(CreateInstance());

		if (vkContext_.enableValidationLayers)
		{
			VK_INIT_STEP(SetupDebugMessenger());
		}

		VK_INIT_STEP(CreateSurface());
		VK_INIT_STEP(PickPhysicalDevice());
		VK_INIT_STEP(CreateLogicalDevice());
		VK_INIT_STEP(CreateSwapChain());
		VK_INIT_STEP(CreateImageViews());
		VK_INIT_STEP(CreateGraphicsPipeline());
		VK_INIT_STEP(CreateCommandPool());
		VK_INIT_STEP(CreateCommandBuffer());
		CreateSyncObjects();

		return true;
	}
#undef VK_INIT_STEP

	vk::Result CreateInstance()
	{
		if (vkContext_.enableValidationLayers && !AreValidationLayersSupported(vkContext_.context, kValidationLayers))
		{
			return vk::Result::eErrorLayerNotPresent;
		}

		std::vector<const char*> instanceExtensions(kRequiredInstanceExtensions.begin(), kRequiredInstanceExtensions.end());
		if (vkContext_.enableValidationLayers)
		{
			instanceExtensions.emplace_back(vk::EXTDebugUtilsExtensionName);
		}

		if (!AreInstanceExtensionsSupported(vkContext_.context, instanceExtensions))
		{
			return vk::Result::eErrorExtensionNotPresent;
		}

		constexpr vk::ApplicationInfo appInfo
		{
			.pApplicationName   { "Hello Triangle" },
			.applicationVersion { VK_MAKE_VERSION(1, 0, 0) },
			.pEngineName        { "GroveEngine" },
			.engineVersion      { VK_MAKE_VERSION(1, 0, 0) },
			.apiVersion         { vk::ApiVersion13 }
		};

		std::vector<const char*> layers;
		if (vkContext_.enableValidationLayers)
		{
			layers.assign(kValidationLayers.begin(), kValidationLayers.end());
		}

		vk::InstanceCreateInfo instanceInfo{};
		instanceInfo.setPApplicationInfo(&appInfo)
			      .setPEnabledLayerNames(layers)
		          .setPEnabledExtensionNames(instanceExtensions);

		auto instance = vkContext_.context.createInstance(instanceInfo);
		VK_CHECK(instance, "vk.instance.create.failed");

		vkContext_.instance = std::move(*instance);
		return vk::Result::eSuccess;
	}

	vk::Result SetupDebugMessenger()
	{
		GRV_ASSERT(vkContext_.enableValidationLayers);

		vk::DebugUtilsMessengerCreateInfoEXT debugInfo{ GetDebugUtilsCreateInfo() };

		auto dbg = vkContext_.instance.createDebugUtilsMessengerEXT(debugInfo);
		VK_CHECK(dbg, "vk.debugUtils.create.failed");
		vkContext_.debugMessenger = std::move(*dbg);

		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.debugMessenger.created");
		return vk::Result::eSuccess;
	}

	vk::Result CreateSurface()
	{
		auto* win32Window{ static_cast<grove::Win32Window*>(window_.get()) };

		const vk::Win32SurfaceCreateInfoKHR surfaceInfo
		{
			.hinstance { win32Window->GetHInstance() },
			.hwnd      { win32Window->GetHWND() }
		};

		auto surface = vkContext_.instance.createWin32SurfaceKHR(surfaceInfo);
		VK_CHECK(surface, "vk.win32Surface.create.failed");

		vkContext_.surface = std::move(*surface);
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.surface.created");
		return vk::Result::eSuccess;
	}

	vk::Result PickPhysicalDevice()
	{
		auto devices = vkContext_.instance.enumeratePhysicalDevices();
		VK_CHECK(devices, "vk.physicalDevices.enumerate.failed");

		if (devices->empty())
		{
			return vk::Result::eErrorInitializationFailed;
		}

		std::multimap<grove::u64, vk::raii::PhysicalDevice> candidates;
		for (const auto& device : *devices)
		{
			grove::u64 score{ RateDeviceSuitability(device) };
			candidates.insert(std::make_pair(score, device));
		}

		grove::u64 deviceScore = candidates.rbegin()->first;
		if (deviceScore <= 0)
		{
			return vk::Result::eErrorInitializationFailed;
		}

		vkContext_.physicalDevice = candidates.rbegin()->second;
		vk::PhysicalDeviceProperties deviceProps = vkContext_.physicalDevice.getProperties();
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.physicalDevice.selected deviceName=\"{}\" score={}", std::string_view{deviceProps.deviceName}, deviceScore);
		return vk::Result::eSuccess;
	}

	grove::u64 RateDeviceSuitability(vk::raii::PhysicalDevice physicalDevice)
	{
		vk::PhysicalDeviceProperties       deviceProps { physicalDevice.getProperties() };
		vk::PhysicalDeviceFeatures         deviceFeats { physicalDevice.getFeatures() };
		vk::PhysicalDeviceMemoryProperties memProps    { physicalDevice.getMemoryProperties() };

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);

		if (!indices.IsComplete() ||
			!deviceFeats.geometryShader ||
			!AreDeviceExtensionsSupported(physicalDevice, kDeviceExtensions) ||
			deviceProps.apiVersion <= vk::ApiVersion13
			)
		{
			GRV_LOG_WARN(GRV_CHANNEL(System), "event=vk.device.unsuitable deviceName=\"{}\" apiVersion={} missingQueues={} geometryShader={}",
				std::string_view{deviceProps.deviceName},
				deviceProps.apiVersion,
				!indices.IsComplete(),
				!deviceFeats.geometryShader);
			return 0;
		}

		grove::u64 score{ 0 };
		if (deviceProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			score += 1000;
		}

		vk::DeviceSize vramBytes{ 0 };
		for (size_t i{ 0 }; i < memProps.memoryHeapCount; ++i)
		{
			if (memProps.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal)
			{
				vramBytes += memProps.memoryHeaps[i].size;
			}
		}

		score += GiB(vramBytes);
		score += deviceProps.limits.maxImageDimension2D;

		GRV_LOG_DEBUG(GRV_CHANNEL(System), "event=vk.device.scoreBreakdown deviceName='{}' type={} totalScore={} discreteBonus={} vramGiB={} maxImageDim={}",
			std::string_view{deviceProps.deviceName},
			vk::to_string(deviceProps.deviceType),
			score,
			(deviceProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1000 : 0),
			GiB(vramBytes), // NOTE: vramBytes is unsigned, which means this is 0 if < 1GB
			deviceProps.limits.maxImageDimension2D);
		return score;
	}

	vk::Result CreateLogicalDevice()
	{
		vkContext_.queueFamilyIndices = FindQueueFamilies(vkContext_.physicalDevice);
		GRV_LOG_TRACE(GRV_CHANNEL(System), "event=vk.queueFamilies.found graphicsFamily={} presentFamily={}",
			vkContext_.queueFamilyIndices.graphicsFamily.value_or(-1),
			vkContext_.queueFamilyIndices.presentFamily.value_or(-1)
			);

		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<grove::u32> uniqueQueueFamilies
		{
			vkContext_.queueFamilyIndices.graphicsFamily.value(),
			vkContext_.queueFamilyIndices.presentFamily.value()
		};

		grove::f32 queuePriority{ 1.0f };
		for (grove::u32 queueFamily : uniqueQueueFamilies)
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

		using Chain = vk::StructureChain<
			vk::DeviceCreateInfo,
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		>;

		Chain chain{};

		chain.get<vk::PhysicalDeviceVulkan11Features>()
			.setShaderDrawParameters(true);

		chain.get<vk::PhysicalDeviceVulkan13Features>()
			.setDynamicRendering(true)
			.setSynchronization2(true);

		chain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
			.setExtendedDynamicState(true);

		auto& deviceInfo = chain.get<vk::DeviceCreateInfo>()
			.setPEnabledExtensionNames(kDeviceExtensions)
			.setQueueCreateInfos(queueCreateInfos);

		auto device = vkContext_.physicalDevice.createDevice(deviceInfo);
		VK_CHECK(device, "vk.logicalDevice.createDevice.failed");
		vkContext_.device = std::move(*device);

		auto graphicsQueue = vkContext_.device.getQueue(vkContext_.queueFamilyIndices.graphicsFamily.value(), 0);
		VK_CHECK(graphicsQueue, "vk.logicalDevice.getGraphicsQueue.failed");
		vkContext_.graphicsQueue = std::move(*graphicsQueue);

		auto presentQueue  = vkContext_.device.getQueue(vkContext_.queueFamilyIndices.presentFamily.value(), 0);
		VK_CHECK(presentQueue, "vk.logicalDevice.getPresentQueue.failed");
		vkContext_.presentQueue = std::move(*presentQueue);

		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.logicalDevice.created graphicsFamily={} presentFamily={} graphicsQueueIndex={} presentQueueIndex={}",
			vkContext_.queueFamilyIndices.graphicsFamily.value(),
			vkContext_.queueFamilyIndices.presentFamily.value(),
			0,
			0);
		return vk::Result::eSuccess;
	}

	vk::Result CreateSwapChain()
	{
		SwapChainSupportDetails swapChainDetails = GetSwapChainSupportDetails();

		vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainDetails);
		vk::PresentModeKHR presentMode = ChooseSwapPresentMode(swapChainDetails, VSyncSetting::OFF);
		vk::Extent2D extent = ChooseSwapExtent(swapChainDetails);

		auto& caps = swapChainDetails.capabilities;

		grove::u32 imageCount{ swapChainDetails.capabilities.minImageCount + 1 };
		if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
		{
			imageCount = caps.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapChainInfo;
		swapChainInfo
			.setFlags(vk::SwapchainCreateFlagsKHR())
			.setSurface(vkContext_.surface)
			.setMinImageCount(imageCount)
			.setImageFormat(surfaceFormat.format)
			.setImageColorSpace(surfaceFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

		if (vkContext_.queueFamilyIndices.graphicsFamily != vkContext_.queueFamilyIndices.presentFamily)
		{
			grove::u32 queueFamilyIndices[]
			{
				vkContext_.queueFamilyIndices.graphicsFamily.value(),
				vkContext_.queueFamilyIndices.presentFamily.value()
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
			.setPreTransform(caps.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(presentMode)
			.setClipped(vk::True)
			.setOldSwapchain(VK_NULL_HANDLE);

		auto swapChain = vkContext_.device.createSwapchainKHR(swapChainInfo);
		VK_CHECK(swapChain, "vk.createSwapChain.failed");

		vkContext_.swapChain              = std::move(*swapChain);
		vkContext_.swapChainSurfaceFormat = std::move(surfaceFormat);
		vkContext_.swapChainExtent        = std::move(extent);
		vkContext_.swapChainImages        = vkContext_.swapChain.getImages();

		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.swapChain.created format={} extent={}x{} imageCount={}",
			vk::to_string(surfaceFormat.format),
			extent.width,
			extent.height,
			vkContext_.swapChainImages.size());
		return vk::Result::eSuccess;
	}

	vk::Result CreateImageViews()
	{
		vkContext_.swapChainImageViews.clear();

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
			.setFormat(vkContext_.swapChainSurfaceFormat.format)
			.setComponents(componentMapping)
			.setSubresourceRange(subresourceRange);

		vkContext_.swapChainImageViews.reserve(vkContext_.swapChainImages.size());
		for (const vk::Image image : vkContext_.swapChainImages)
		{
			imgViewInfo.setImage(image);

			auto imageView = vkContext_.device.createImageView(imgViewInfo);
			VK_CHECK(imageView, "vk.createImageView.failed");

			vkContext_.swapChainImageViews.emplace_back(std::move(*imageView));
		}

		return vk::Result::eSuccess;
	}

	vk::Result CreateGraphicsPipeline()
	{
		auto shaderCode = ReadFile("assets/shaders/slang.spv");
		if (!shaderCode)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.shader.read.failed msg='{}'", shaderCode.error());
			return vk::Result::eErrorInitializationFailed;
		}

		auto shaderModuleExp = CreateShaderModule(*shaderCode);
		if (!shaderModuleExp)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.shaderModule.create.failed msg='{}'", vk::to_string(shaderModuleExp.error()));
			return shaderModuleExp.error();
		}

		vk::raii::ShaderModule shaderModule = std::move(*shaderModuleExp);

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

		auto pipelineLayout = vkContext_.device.createPipelineLayout(pipelineLayoutInfo);
		VK_CHECK(pipelineLayout, "vk.createPipelineLayout.failed");
		vkContext_.pipelineLayout = std::move(*pipelineLayout);

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
			.setLayout(vkContext_.pipelineLayout)
			.setRenderPass(nullptr);

		auto& pipelineRenderingInfo = pipelineInfoChain.get<vk::PipelineRenderingCreateInfo>();
		pipelineRenderingInfo
			.setColorAttachmentFormats(vkContext_.swapChainSurfaceFormat.format);

		auto graphicsPipeline = vkContext_.device.createGraphicsPipeline(nullptr, graphicsPipelineInfo);
		VK_CHECK(graphicsPipeline, "vk.createGraphicsPipeline.failed");

		vkContext_.graphicsPipeline = std::move(*graphicsPipeline);
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.graphicsPipeline.created");
		return vk::Result::eSuccess;
	}

	vk::Result CreateCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{};
		poolInfo
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			.setQueueFamilyIndex(vkContext_.queueFamilyIndices.graphicsFamily.value());

		auto commandPool = vkContext_.device.createCommandPool(poolInfo);
		VK_CHECK(commandPool, "vk.createCommandPool.failed");

		vkContext_.commandPool = std::move(*commandPool);
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.commandPool.created graphicsFamilyIndex={}", poolInfo.queueFamilyIndex);
		return vk::Result::eSuccess;
	}

	vk::Result CreateCommandBuffer()
	{
		vk::CommandBufferAllocateInfo allocInfo{};
		allocInfo
			.setCommandPool(vkContext_.commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);

		auto commandBuffer = vkContext_.device.allocateCommandBuffers(allocInfo);
		VK_CHECK(commandBuffer, "vk.createCommandBuffer.failed");

		vkContext_.commandBuffer = std::move(commandBuffer->front());
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.commandBuffer.created level={} bufferCount={}", vk::to_string(vk::CommandBufferLevel::ePrimary), 1);
		return vk::Result::eSuccess;
	}

	void CreateSyncObjects()
	{
		auto presentCompleteSemaphore = vkContext_.device.createSemaphore(vk::SemaphoreCreateInfo());
		if (!presentCompleteSemaphore)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.createPresentCompleteSemaphore.failed result={}", vk::to_string(presentCompleteSemaphore.error()));
		}
		else
		{
			vkContext_.presentCompleteSemaphore = std::move(*presentCompleteSemaphore);
		}

		auto renderingFinishedSemaphore = vkContext_.device.createSemaphore(vk::SemaphoreCreateInfo());
		if (!renderingFinishedSemaphore)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.createRenderingFinishedSemaphore.failed result={}", vk::to_string(renderingFinishedSemaphore.error()));
		}
		else
		{
			vkContext_.renderFinishedSemaphore = std::move(*renderingFinishedSemaphore);
		}

		auto drawFence = vkContext_.device.createFence(
			{
				.flags = vk::FenceCreateFlagBits::eSignaled
			}
		);
		if (!drawFence)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.createDrawFence.failed result={}", vk::to_string(drawFence.error()));
		}
		else
		{
			vkContext_.drawFence = std::move(*drawFence);
		}
	}

	void RecordCommandBuffer(grove::u32 imageIndex)
	{
		vkContext_.commandBuffer.begin({});

		TransitionImageLayout(
			imageIndex,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput
		);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo{};
		attachmentInfo
			.setImageView(vkContext_.swapChainImageViews[imageIndex])
			.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setClearValue(clearColor);

		vk::RenderingInfo renderingInfo{};
		renderingInfo
			.setRenderArea(
				{
					.offset { 0, 0 },
					.extent { vkContext_.swapChainExtent }
				}
			)
			.setLayerCount(1)
			.setColorAttachments(attachmentInfo);

		vkContext_.commandBuffer.beginRendering(renderingInfo);
		vkContext_.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vkContext_.graphicsPipeline);

		vk::Viewport viewport
		{
			.x        { 0.0f },
			.y        { 0.0f },
			.width    { static_cast<grove::f32>(vkContext_.swapChainExtent.width) },
			.height   { static_cast<grove::f32>(vkContext_.swapChainExtent.height) },
			.minDepth { 0.0f },
			.maxDepth { 1.0f }
		};

		vkContext_.commandBuffer.setViewport(0, viewport);
		vkContext_.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vkContext_.swapChainExtent));
		vkContext_.commandBuffer.draw(3, 1, 0, 0);

		vkContext_.commandBuffer.endRendering();

		TransitionImageLayout(
			imageIndex,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe
		);

		vkContext_.commandBuffer.end();
	}

	void TransitionImageLayout(
		grove::u32 imageIndex,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask,
		vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask,
		vk::PipelineStageFlags2 dstStageMask
	)
	{
		vk::ImageMemoryBarrier2 barrier{};
		barrier
			.setSrcAccessMask(srcAccessMask)
			.setSrcStageMask(srcStageMask)
			.setDstAccessMask(dstAccessMask)
			.setDstStageMask(dstStageMask)
			.setOldLayout(oldLayout)
			.setNewLayout(newLayout)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(vkContext_.swapChainImages[imageIndex])
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

		vkContext_.commandBuffer.pipelineBarrier2(dependencyInfo);
	}

	void MainLoop()
	{
		// TODO: get back to this when input system is ready
		while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(window_->GetNativeHandle())))
		{
			window_->OnUpdate();

			if (auto result = DrawFrame(); result != vk::Result::eSuccess)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.drawFrame.failed result={}", vk::to_string(result));
			}
		}

		vkContext_.device.waitIdle();
	}

	vk::Result DrawFrame()
	{
		vkContext_.graphicsQueue.waitIdle();
		vkContext_.presentQueue.waitIdle();

		auto [result, imageIndex] = vkContext_.swapChain.acquireNextImage(UINT64_MAX, *vkContext_.presentCompleteSemaphore, nullptr);
		RecordCommandBuffer(imageIndex);

		vkContext_.device.resetFences(*vkContext_.drawFence);

		// submit for drawing
		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::SubmitInfo submitInfo{};
		submitInfo
			.setWaitSemaphores(*vkContext_.presentCompleteSemaphore)
			.setWaitDstStageMask(waitDestinationStageMask)
			.setCommandBuffers(*vkContext_.commandBuffer)
			.setSignalSemaphores(*vkContext_.renderFinishedSemaphore);

		vkContext_.graphicsQueue.submit(submitInfo, *vkContext_.drawFence);

		// submit for presentation
		result = vkContext_.device.waitForFences(*vkContext_.drawFence, vk::True, UINT64_MAX);
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.waitForDrawFence.failed result={}", vk::to_string(result));
			return result;
		}

		vk::PresentInfoKHR presentInfoKHR{};
		presentInfoKHR
			.setWaitSemaphores(*vkContext_.renderFinishedSemaphore)
			.setSwapchains(*vkContext_.swapChain)
			.setImageIndices(imageIndex);

		result = vkContext_.presentQueue.presentKHR(presentInfoKHR);
		return result;
	}

	void Cleanup()
	{
		window_.reset();
		engine_.Shutdown();
	}

	QueueFamilyIndices FindQueueFamilies(vk::raii::PhysicalDevice physicalDevice)
	{
		QueueFamilyIndices indices{};

		std::vector<vk::QueueFamilyProperties> queueFamilies{ physicalDevice.getQueueFamilyProperties() };

		for (grove::u32 i{ 0 }; i < queueFamilies.size(); ++i)
		{
			if (indices.IsComplete())
			{
				break;
			}

			if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0))
			{
				indices.graphicsFamily = i;
			}

			vk::Bool32 presentSupport = physicalDevice.getSurfaceSupportKHR(i, vkContext_.surface);
			if (presentSupport)
			{
				indices.presentFamily = i;
			}
		}

		return indices;
	}

	vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const SwapChainSupportDetails& swapChainDetails)
	{
		for (const vk::SurfaceFormatKHR& format : swapChainDetails.formats)
		{
			// pick srgb if available
			if (format.format == vk::Format::eB8G8R8A8Srgb &&
				format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				return format;
			}
		}

		return swapChainDetails.formats.front();
	}

	vk::PresentModeKHR ChooseSwapPresentMode(const SwapChainSupportDetails& swapChainDetails, VSyncSetting vsyncSetting)
	{

		vk::PresentModeKHR requestedMode = ToPresentMode(vsyncSetting);

		for (vk::PresentModeKHR presentMode : swapChainDetails.presentModes)
		{
			if (presentMode == requestedMode)
			{
				return presentMode;
			}
		}

		// fifo guaranteed by spec
		GRV_LOG_WARN(GRV_CHANNEL(System), 
			"event=vk.swapChain.presentModeUnavailable requested=\"{}\" requestedMode=\"{}\" fallback=\"FIFO\"",
			ToString(vsyncSetting),
			vk::to_string(requestedMode));

		return vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D ChooseSwapExtent(const SwapChainSupportDetails& swapChainDetails)
	{
		const auto& caps = swapChainDetails.capabilities;

		if (caps.currentExtent.width != std::numeric_limits<grove::u32>::max())
		{
			return caps.currentExtent;
		}

		return
		{
			std::clamp<grove::u32>(window_->GetWidth(), caps.minImageExtent.width, caps.maxImageExtent.width),
			std::clamp<grove::u32>(window_->GetHeight(), caps.minImageExtent.height, caps.maxImageExtent.height)
		};
	}

	[[nodiscard]] std::expected<vk::raii::ShaderModule, vk::Result> CreateShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo shaderModuleInfo
		{
			.codeSize { code.size() * sizeof(char) },
			.pCode    { reinterpret_cast<const grove::u32*>(code.data()) }
		};

		return vkContext_.device.createShaderModule(shaderModuleInfo);
	}

	SwapChainSupportDetails GetSwapChainSupportDetails()
	{
		SwapChainSupportDetails outDetails{};

		outDetails.capabilities = vkContext_.physicalDevice.getSurfaceCapabilitiesKHR(vkContext_.surface);
		outDetails.formats = vkContext_.physicalDevice.getSurfaceFormatsKHR(vkContext_.surface);
		outDetails.presentModes = vkContext_.physicalDevice.getSurfacePresentModesKHR(vkContext_.surface);

		return outDetails;
	}

	vk::DebugUtilsMessengerCreateInfoEXT GetDebugUtilsCreateInfo()
	{
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags
		{
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		};

		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags
		{
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		};

		vk::DebugUtilsMessengerCreateInfoEXT debugInfo
		{
			.messageSeverity { severityFlags },
			.messageType     { messageTypeFlags },
			.pfnUserCallback { &DebugCallback }
		};

		return debugInfo;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
		vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* userData
	)
	{
		if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
		{
			GRV_LOG_TRACE(GRV_CHANNEL(System), "event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
		{
			GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			GRV_LOG_WARN(GRV_CHANNEL(System), "event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}

		return vk::False;
	}

private:
	grove::GroveEngine             engine_;
	std::unique_ptr<grove::Window> window_;
	VulkanContext                  vkContext_;
};

int main()
{
	HelloTriangleApplication app;

	if (!app.Run())
	{
		GRV_LOG_FATAL(GRV_CHANNEL(System), "event=app.initFailed");
		GRV_DEBUG_BREAK();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
