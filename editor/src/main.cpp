#include <array>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string.h>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#include <span>
#include <unordered_map>
#include <ranges>
#include <limits>

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
#include "grove/platform/glfw_window.hpp"

#define KiB(x) ((x) >> 10)
#define MiB(x) ((x) >> 20)
#define GiB(x) ((x) >> 30)

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
	case VSyncSetting::OFF:              return "OFF";
	case VSyncSetting::ON:               return "ON";
	case VSyncSetting::ADAPTIVE:         return "ADAPTIVE";
	case VSyncSetting::TRIPLE_BUFFERING: return "TRIPLE_BUFFERING";
	default:                             GRV_ASSERT(true, "Invalid VSyncSetting");
	}
}

constexpr vk::PresentModeKHR ToPresentMode(VSyncSetting setting)
{
	switch (setting)
	{
	case VSyncSetting::OFF:              return vk::PresentModeKHR::eImmediate;
	case VSyncSetting::ON:               return vk::PresentModeKHR::eFifo;
	case VSyncSetting::ADAPTIVE:         return vk::PresentModeKHR::eFifoRelaxed;
	case VSyncSetting::TRIPLE_BUFFERING: return vk::PresentModeKHR::eMailbox;
	default:                             GRV_ASSERT(true, "Invalid VSyncSetting");
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
		GetNameFunc getName)
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
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Required {} '{}' is not available.", typeName, missing);
			}
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Missing {} required {}(s).", requiredSet.size(), typeName);
			return false;
		}

		return true;
	}

	bool AreValidationLayersSupported(std::span<const char* const> layers)
	{
		auto [result, availableLayers] = vk::enumerateInstanceLayerProperties();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to enumerate Vulkan Instance Layer Properties: {}", vk::to_string(result));
			return false;
		}

		return ValidateSupport<vk::LayerProperties>(
			std::span{availableLayers},
			layers,
			"validation layer",
			[](const vk::LayerProperties& prop) -> std::string_view { return prop.layerName; }
		);
	}

	bool AreInstanceExtensionsSupported(std::span<const char* const> extensions)
	{
		auto [result, availableExtensions] = vk::enumerateInstanceExtensionProperties();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to enumerate Vulkan instance extension properties: {}", vk::to_string(result));
			return false;
		}

		return ValidateSupport<vk::ExtensionProperties>(
			std::span{availableExtensions},
			extensions,
			"instance extension",
			[](const vk::ExtensionProperties& prop) -> std::string_view { return prop.extensionName; }
		);
	}

	bool AreDeviceExtensionsSupported(vk::raii::PhysicalDevice device, std::span<const char* const> extensions)
	{
		auto [result, availableExtensions] = device.enumerateDeviceExtensionProperties();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to enumerate device extension properties: {}", vk::to_string(result));
			return false;
		}

		return ValidateSupport<vk::ExtensionProperties>(
			std::span{availableExtensions},
			extensions,
			"device extension",
			[](const vk::ExtensionProperties& prop) -> std::string_view { return prop.extensionName; }
		);
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
	vk::raii::Instance               instance               { nullptr };
	vk::raii::DebugUtilsMessengerEXT debugMessenger         { nullptr };
	vk::raii::SurfaceKHR             surface                { nullptr };
	vk::raii::PhysicalDevice         physicalDevice         { nullptr };
	vk::raii::Device                 device                 { nullptr };
	QueueFamilyIndices               queueFamilyIndices     { };
	vk::raii::SwapchainKHR           swapChain              { nullptr };
	vk::raii::Queue                  graphicsQueue          { nullptr };
	vk::raii::Queue                  presentQueue           { nullptr };
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
		grove_ = grove::BoxPtr<grove::GroveEngine>::Create();
		grove_->Init();

		GRV_SET_LOG_LEVEL(trace);
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

	bool InitVulkan()
	{
		vk::Result result = CreateInstance();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to create Vulkan Instance: {}", vk::to_string(result));
			return false;
		}

		result = SetupDebugMessenger();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to setup Vulkan Debug Messenger: {}", vk::to_string(result));
			return false;
		}

		result = CreateSurface();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to create Vulkan Surface: {}", vk::to_string(result));
			return false;
		}

		result = PickPhysicalDevice();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to pick Vulkan Physical Device: {}", vk::to_string(result));
			return false;
		}

		result = CreateLogicalDevice();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to pick create Vulkan Logical Device: {}", vk::to_string(result));
			return false;
		}

		result = CreateSwapChain();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to create Vulkan SwapChain: {}", vk::to_string(result));
			return false;
		}

		return true;
	}

	vk::Result CreateInstance()
	{
		if (vulkanContext_.enableValidationLayers && !AreValidationLayersSupported(kValidationLayers))
		{
			return vk::Result::eErrorLayerNotPresent;
		}

		std::vector<const char*> extensions(kRequiredInstanceExtensions.begin(), kRequiredInstanceExtensions.end());
		if (vulkanContext_.enableValidationLayers)
		{
			extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
		}

		if (!AreInstanceExtensionsSupported(extensions))
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
		if (vulkanContext_.enableValidationLayers)
		{
			layers.assign(kValidationLayers.begin(), kValidationLayers.end());
		}

		vk::InstanceCreateInfo createInfo{};
		createInfo.setPApplicationInfo(&appInfo)
			      .setPEnabledLayerNames(layers)
		          .setPEnabledExtensionNames(extensions);

		auto [result, instance] = vk::createInstance(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		vulkanContext_.instance = vk::raii::Instance(vulkanContext_.context, instance);
		return vk::Result::eSuccess;
	}

	vk::Result SetupDebugMessenger()
	{
		GRV_ASSERT(vulkanContext_.enableValidationLayers);

		vk::DebugUtilsMessengerCreateInfoEXT createInfo{ GetDebugUtilsCreateInfo() };

		auto [result, dbg] = vulkanContext_.instance.createDebugUtilsMessengerEXT(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		vulkanContext_.debugMessenger = std::move(dbg);
		return vk::Result::eSuccess;
	}

	vk::Result CreateSurface()
	{
		auto* glfwWindow{ static_cast<grove::GLFWWindow*>(window_.Get())};

		const vk::Win32SurfaceCreateInfoKHR createInfo
		{
			.hinstance { glfwWindow-> GetHInstance() },
			.hwnd      { glfwWindow-> GetHWND() }
		};

		auto [result, surf] = vulkanContext_.instance.createWin32SurfaceKHR(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		vulkanContext_.surface = std::move(surf);
		return vk::Result::eSuccess;
	}

	vk::Result PickPhysicalDevice()
	{
		auto [result, devices] = vulkanContext_.instance.enumeratePhysicalDevices();
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		if (devices.empty())
		{
			return vk::Result::eErrorInitializationFailed;
		}

		std::multimap<grove::u64, vk::raii::PhysicalDevice> candidates;
		for (const auto& device : devices)
		{
			grove::u64 score{ RateDeviceSuitability(device) };
			candidates.insert(std::make_pair(score, device));
		}

		if (candidates.rbegin()->first <= 0)
		{
			return vk::Result::eErrorInitializationFailed;
		}

		vulkanContext_.physicalDevice = candidates.rbegin()->second;
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
			return 0;
		}

		grove::u64 score{ 0 };

		// prefer discrete gpu
		if (deviceProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			score += 1000;
		}

		// prefer highest vram
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

		return score;
	}

	vk::Result CreateLogicalDevice()
	{
		vulkanContext_.queueFamilyIndices = FindQueueFamilies(vulkanContext_.physicalDevice);

		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<grove::u32> uniqueQueueFamilies
		{
			vulkanContext_.queueFamilyIndices.graphicsFamily.value(),
			vulkanContext_.queueFamilyIndices.presentFamily.value()
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

		auto [result, dev] = vulkanContext_.physicalDevice.createDevice(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		vulkanContext_.device        = std::move(dev);
		vulkanContext_.graphicsQueue = vulkanContext_.device.getQueue(vulkanContext_.queueFamilyIndices.graphicsFamily.value(), 0);
		vulkanContext_.presentQueue  = vulkanContext_.device.getQueue(vulkanContext_.queueFamilyIndices.presentFamily.value(), 0);

		return vk::Result::eSuccess;
	}

	vk::Result CreateSwapChain()
	{
		SwapChainSupportDetails swapChainDetails{};
		GetSwapChainSupportDetails(swapChainDetails);

		vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainDetails);
		vk::PresentModeKHR   presentMode   = ChooseSwapPresentMode(swapChainDetails, VSyncSetting::OFF);
		vk::Extent2D         extent        = ChooseSwapExtent(swapChainDetails);

		auto& caps = swapChainDetails.capabilities;

		grove::u32 imageCount{ swapChainDetails.capabilities.minImageCount + 1 };
		if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
		{
			imageCount = caps.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR createInfo;
		createInfo
			.setFlags(vk::SwapchainCreateFlagsKHR())
			.setSurface(vulkanContext_.surface)
			.setMinImageCount(imageCount)
			.setImageFormat(surfaceFormat.format)
			.setImageColorSpace(surfaceFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

		if (vulkanContext_.queueFamilyIndices.graphicsFamily != vulkanContext_.queueFamilyIndices.presentFamily)
		{
			grove::u32 queueFamilyIndices[]
			{
				vulkanContext_.queueFamilyIndices.graphicsFamily.value(),
				vulkanContext_.queueFamilyIndices.presentFamily.value()
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

		auto [res, swapChain] = vulkanContext_.device.createSwapchainKHR(createInfo);
		if (res != vk::Result::eSuccess)
		{
			return res;
		}

		vulkanContext_.swapChain = std::move(swapChain);
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
		window_.Reset();

		if (grove_)
		{
			grove_->Shutdown();
			grove_.Reset();
		}
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

			auto [res, presentSupport] = physicalDevice.getSurfaceSupportKHR(i, vulkanContext_.surface);

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
			if (format.format == vk::Format::eR8G8B8A8Srgb &&
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
			"VSync setting '{}' (mode '{}') is not available. Falling back to FIFO",
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

	vk::Result GetSwapChainSupportDetails(SwapChainSupportDetails& outDetails)
	{
		{
			auto [res, caps] = vulkanContext_.physicalDevice.getSurfaceCapabilitiesKHR(vulkanContext_.surface);
			if (res != vk::Result::eSuccess)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to get surface capabilities: {}", vk::to_string(res));
				return res;
			}
			outDetails.capabilities = std::move(caps);
		}

		{
			auto [res, fmts] = vulkanContext_.physicalDevice.getSurfaceFormatsKHR(vulkanContext_.surface);
			if (res != vk::Result::eSuccess)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to get surface formats: {}", vk::to_string(res));
				return res;
			}
			outDetails.formats = std::move(fmts);
		}

		{
			auto [res, modes] = vulkanContext_.physicalDevice.getSurfacePresentModesKHR(vulkanContext_.surface);
			if (res != vk::Result::eSuccess)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to get surface present modes: {}", vk::to_string(res));
				return res;
			}
			outDetails.presentModes = std::move(modes);
		}

		return vk::Result::eSuccess;
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
			GRV_LOG_TRACE(GRV_CHANNEL(System), "{} - {}: {}", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
		{
			GRV_LOG_INFO(GRV_CHANNEL(System), "{} - {}: {}", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			GRV_LOG_WARN(GRV_CHANNEL(System), "{} - {}: {}", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}
		else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "{} - {}: {}", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
		}

		return vk::False;
	}

private:
	grove::BoxPtr<grove::GroveEngine> grove_;
	grove::BoxPtr<grove::Window>      window_;
	VulkanContext vulkanContext_;
};

int main()
{
	HelloTriangleApplication app;

	if (!app.Run())
	{
		GRV_LOG_FATAL(GRV_CHANNEL(System), "Application failed to start due to initialization errors.");
		GRV_DEBUG_BREAK();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
