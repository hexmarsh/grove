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

#define VK_CHECK(result) \
do { \
	if (!result)\
	{\
		GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.error result={}", vk::to_string(result.error()));\
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
	vk::raii::Instance               instance               { nullptr };
	vk::raii::DebugUtilsMessengerEXT debugMessenger         { nullptr };
	vk::raii::SurfaceKHR             surface                { nullptr };

	// device level
	vk::raii::PhysicalDevice         physicalDevice         { nullptr };
	vk::raii::Device                 device                 { nullptr };
	QueueFamilyIndices               queueFamilyIndices     { };

	// swap chain & queues
	vk::raii::SwapchainKHR           swapChain              { nullptr };
	vk::raii::Queue                  graphicsQueue          { nullptr };
	vk::raii::Queue                  presentQueue           { nullptr };
	std::vector<vk::Image>           swapChainImages;
	std::vector<vk::raii::ImageView> swapChainImageViews;
	vk::Format                       swapChainImageFormat   { vk::Format::eUndefined };
	vk::Extent2D                     swapChainExtent;

	#if defined(NDEBUG)
	bool                             enableValidationLayers { false };
	#else
	bool                             enableValidationLayers { true };
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

#define VK_INIT_STEP(expr, msg) \
do { \
	if (auto r = (expr); r != vk::Result::eSuccess) {\
		GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.init.failed step=\"{}\" result={}", (msg), vk::to_string(r)); \
		return false; \
	} \
} while (0)

	bool InitVulkan()
	{
		VK_INIT_STEP(CreateInstance(), "Failed to create Vulkan Instance");

		if (vkContext_.enableValidationLayers)
		{
			VK_INIT_STEP(SetupDebugMessenger(), "Failed to setup Vulkan Debug Messenger");
		}

		VK_INIT_STEP(CreateSurface(),          "Failed to create Vulkan Surface");
		VK_INIT_STEP(PickPhysicalDevice(),     "Failed to pick Vulkan Physical Device");
		VK_INIT_STEP(CreateLogicalDevice(),    "Failed to create Vulkan Logical Device");
		VK_INIT_STEP(CreateSwapChain(),        "Failed to create Vulkan SwapChain");
		VK_INIT_STEP(CreateImageViews(),       "Failed to create Vulkan Image Views");
		VK_INIT_STEP(CreateGraphicsPipeline(), "Failed to create Vulkan Graphics Pipeline");
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
			.apiVersion         { vk::ApiVersion14 }
		};

		std::vector<const char*> layers;
		if (vkContext_.enableValidationLayers)
		{
			layers.assign(kValidationLayers.begin(), kValidationLayers.end());
		}

		vk::InstanceCreateInfo createInfo{};
		createInfo.setPApplicationInfo(&appInfo)
			      .setPEnabledLayerNames(layers)
		          .setPEnabledExtensionNames(instanceExtensions);

		auto instance = vkContext_.context.createInstance(createInfo);
		VK_CHECK(instance);

		vkContext_.instance = std::move(*instance);
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.instance.created apiVersion={}", vk::ApiVersion14);
		return vk::Result::eSuccess;
	}

	vk::Result SetupDebugMessenger()
	{
		GRV_ASSERT(vkContext_.enableValidationLayers);

		vk::DebugUtilsMessengerCreateInfoEXT createInfo{ GetDebugUtilsCreateInfo() };

		auto dbg = vkContext_.instance.createDebugUtilsMessengerEXT(createInfo);
		VK_CHECK(dbg);
		vkContext_.debugMessenger = std::move(*dbg);

		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.debugMessenger.created");
		return vk::Result::eSuccess;
	}

	vk::Result CreateSurface()
	{
		auto* win32Window{ static_cast<grove::Win32Window*>(window_.get()) };

		const vk::Win32SurfaceCreateInfoKHR createInfo
		{
			.hinstance { win32Window->GetHInstance() },
			.hwnd      { win32Window->GetHWND() }
		};

		auto surf = vkContext_.instance.createWin32SurfaceKHR(createInfo);
		VK_CHECK(surf);

		vkContext_.surface = std::move(*surf);
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=vk.surface.created");
		return vk::Result::eSuccess;
	}

	vk::Result PickPhysicalDevice()
	{
		auto devices = vkContext_.instance.enumeratePhysicalDevices();
		VK_CHECK(devices);

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
			!AreDeviceExtensionsSupported(physicalDevice, kDeviceExtensions))
		{
			GRV_LOG_WARN(GRV_CHANNEL(System), "event=vk.device.unsuitable deviceName=\"{}\" missingQueues={} geometryShader={}",
				std::string_view{deviceProps.deviceName},
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
			GiB(vramBytes),
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
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		>;

		Chain chain{};

		chain.get<vk::PhysicalDeviceVulkan13Features>()
			.setDynamicRendering(true);

		chain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
			.setExtendedDynamicState(true);

		auto& createInfo = chain.get<vk::DeviceCreateInfo>()
			.setPEnabledExtensionNames(kDeviceExtensions)
			.setQueueCreateInfos(queueCreateInfos);

		auto device = vkContext_.physicalDevice.createDevice(createInfo);
		VK_CHECK(device);
		vkContext_.device        = std::move(*device);

		auto graphicsQueue = vkContext_.device.getQueue(vkContext_.queueFamilyIndices.graphicsFamily.value(), 0);
		VK_CHECK(graphicsQueue);
		vkContext_.graphicsQueue = std::move(*graphicsQueue);

		auto presentQueue  = vkContext_.device.getQueue(vkContext_.queueFamilyIndices.presentFamily.value(), 0);
		VK_CHECK(presentQueue);
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

		vk::SwapchainCreateInfoKHR createInfo;
		createInfo
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

			createInfo
				.setImageSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndexCount(2)
				.setQueueFamilyIndices(queueFamilyIndices);
		}
		else
		{
			createInfo
				.setImageSharingMode(vk::SharingMode::eExclusive)
				.setQueueFamilyIndexCount(0)      // optional
				.setPQueueFamilyIndices(nullptr); // optional
		}

		createInfo
			.setPreTransform(caps.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(presentMode)
			.setClipped(vk::True)
			.setOldSwapchain(VK_NULL_HANDLE);

		auto swapChain = vkContext_.device.createSwapchainKHR(createInfo);
		VK_CHECK(swapChain);

		vkContext_.swapChain = std::move(*swapChain);
		vkContext_.swapChainImageFormat = surfaceFormat.format;
		vkContext_.swapChainExtent = std::move(extent);
		vkContext_.swapChainImages = vkContext_.swapChain.getImages();

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
			.setR(vk::ComponentSwizzle::eIdentity)
			.setG(vk::ComponentSwizzle::eIdentity)
			.setB(vk::ComponentSwizzle::eIdentity)
			.setA(vk::ComponentSwizzle::eIdentity);

		vk::ImageSubresourceRange subresourceRange{};
		subresourceRange
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(1)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		vk::ImageViewCreateInfo createInfo{};
		createInfo
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vkContext_.swapChainImageFormat)
			.setComponents(componentMapping)
			.setSubresourceRange(subresourceRange);

		vkContext_.swapChainImageViews.reserve(vkContext_.swapChainImages.size());
		for (const vk::Image image : vkContext_.swapChainImages)
		{
			createInfo.setImage(image);

			auto imageView = vkContext_.device.createImageView(createInfo);
			VK_CHECK(imageView);

			vkContext_.swapChainImageViews.emplace_back(std::move(*imageView));
		}

		return vk::Result::eSuccess;
	}

	vk::Result CreateGraphicsPipeline()
	{
		auto shaderCode = ReadFile("../engine/assets/shaders/slang.spv");
		if (!shaderCode)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.shader.read.failed msg='{}'", shaderCode.error());
			return vk::Result::eErrorInitializationFailed;
		}

		auto shaderModuleExp = CreateShaderModule(*shaderCode);
		if (!shaderModuleExp)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=vk.shaderModule.create.failed result='{}'", vk::to_string(shaderModuleExp.error()));
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

		return vk::Result::eSuccess;
	}

	void MainLoop()
	{
		// TODO: get back to this when input system is ready
		while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(window_->GetNativeHandle())))
		{
			window_->OnUpdate();
		}
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
		vk::ShaderModuleCreateInfo createInfo
		{
			.codeSize { code.size() * sizeof(char) },
			.pCode    { reinterpret_cast<const grove::u32*>(code.data()) }
		};

		return vkContext_.device.createShaderModule(createInfo);
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

		vk::DebugUtilsMessengerCreateInfoEXT createInfo
		{
			.messageSeverity { severityFlags },
			.messageType     { messageTypeFlags },
			.pfnUserCallback { &DebugCallback }
		};

		return createInfo;
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
