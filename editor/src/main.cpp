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

		return true;
	}

	vk::Result CreateInstance()
	{
		if (enableValidationLayers_ && !AreValidationLayersSupported(kValidationLayers))
		{
			return vk::Result::eErrorLayerNotPresent;
		}

		std::vector<const char*> extensions(kRequiredInstanceExtensions.begin(), kRequiredInstanceExtensions.end());
		if (enableValidationLayers_)
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
		if (enableValidationLayers_)
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

		instance_ = vk::raii::Instance(context_, instance);
		return vk::Result::eSuccess;
	}

	vk::Result SetupDebugMessenger()
	{
		GRV_ASSERT(enableValidationLayers_);

		vk::DebugUtilsMessengerCreateInfoEXT createInfo{ GetDebugUtilsCreateInfo() };

		auto [result, dbg] = instance_.createDebugUtilsMessengerEXT(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		debugMessenger_ = std::move(dbg);
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

		auto [result, surf] = instance_.createWin32SurfaceKHR(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		surface_ = std::move(surf);
		return vk::Result::eSuccess;
	}

	vk::Result PickPhysicalDevice()
	{
		auto [result, devices] = instance_.enumeratePhysicalDevices();
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

		physicalDevice_ = candidates.rbegin()->second;
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
		QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice_) };

		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<grove::u32> uniqueQueueFamilies
		{
			indices.graphicsFamily.value(),
			indices.presentFamily.value()
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

		auto [result, dev] = physicalDevice_.createDevice(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		device_        = std::move(dev);
		graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
		presentQueue_  = device_.getQueue(indices.presentFamily.value(), 0);

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

			auto [res, presentSupport] = physicalDevice.getSurfaceSupportKHR(i, surface_);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}
		}

		return indices;
	}

	vk::Result GetSwapChainSupportDetails(SwapChainSupportDetails& outDetails)
	{
		{
			auto [res, caps] = physicalDevice_.getSurfaceCapabilitiesKHR(surface_);
			if (res != vk::Result::eSuccess)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to get surface capabilities: {}", vk::to_string(res));
				return res;
			}
			outDetails.capabilities = std::move(caps);
		}

		{
			auto [res, fmts] = physicalDevice_.getSurfaceFormatsKHR(surface_);
			if (res != vk::Result::eSuccess)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to get surface formats: {}", vk::to_string(res));
				return res;
			}
			outDetails.formats = std::move(fmts);
		}

		{
			auto [res, modes] = physicalDevice_.getSurfacePresentModesKHR(surface_);
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
	vk::raii::Context                 context_;
	vk::raii::Instance                instance_               { nullptr };
	vk::raii::DebugUtilsMessengerEXT  debugMessenger_         { nullptr };
	vk::raii::SurfaceKHR              surface_                { nullptr };
	vk::raii::PhysicalDevice          physicalDevice_         { nullptr };
	vk::raii::Device                  device_                 { nullptr };
	vk::raii::Queue                   graphicsQueue_          { nullptr };
	vk::raii::Queue                   presentQueue_           { nullptr };
	#if defined(NDEBUG)
	bool                              enableValidationLayers_ { false };
	#else
	bool                              enableValidationLayers_ { true };
	#endif
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
