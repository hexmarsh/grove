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

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include <windows.h>

#include "grove/core/assert.hpp"
#include "grove/core/grove_engine.hpp"
#include "grove/core/logging/log_macros.hpp"
#include "grove/core/memory/box_ptr.hpp"
#include "grove/core/typedefs.hpp"
#include "grove/core/window.hpp"
#include "grove/platform/win32_window.hpp"

#define KiB(x) ((x) >> 10)
#define MiB(x) ((x) >> 20)
#define GiB(x) ((x) >> 30)

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
		vk::Result result { CreateInstance() };
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
		auto validationLayers{ GetValidationLayers() };
		if (!enableValidationLayers_ && !IsValidationLayersSupported(validationLayers))
		{
			return vk::Result::eErrorLayerNotPresent;
		}

		std::vector<const char*> requiredLayers;
		if (enableValidationLayers_)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		std::vector<const char*> extensions{ GetRequiredVulkanExtensions() };
		if (!IsRequiredVulkanExtensionsSupported(extensions))
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

		vk::InstanceCreateInfo createInfo
		{
			.pApplicationInfo        { &appInfo },
			.enabledLayerCount       { static_cast<grove::u32>(requiredLayers.size()) },
			.ppEnabledLayerNames     { requiredLayers.data() },
			.enabledExtensionCount   { static_cast<grove::u32>(extensions.size()) },
			.ppEnabledExtensionNames { extensions.data() }
		};

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
		auto* win32Window{ static_cast<grove::Win32Window*>(window_.Get())};

		const vk::Win32SurfaceCreateInfoKHR createInfo
		{
			.hinstance { win32Window-> GetHInstance() },
			.hwnd      { win32Window-> GetHWND() }
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
			!IsSupportsRequiredDeviceExtensions(physicalDevice))
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
		auto validationLayers{ GetValidationLayers() };
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
			vk::DeviceQueueCreateInfo queueCreateInfo
			{
				.queueFamilyIndex { queueFamily },
				.queueCount       { 1 },
				.pQueuePriorities { &queuePriority }
			};

			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		std::array<const char*, 1> deviceExtensions{ GetRequiredDeviceExtensions() };

		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain
		{
			{ },
			{ .dynamicRendering { true } },
			{ .extendedDynamicState { true } }
		};

		vk::DeviceCreateInfo createInfo
		{
			.pNext                   { &featureChain.get<vk::PhysicalDeviceFeatures2>() },
			.queueCreateInfoCount    { static_cast<grove::u32>(queueCreateInfos.size()) },
			.pQueueCreateInfos       { queueCreateInfos.data() },
			.enabledExtensionCount   { static_cast<grove::u32>(deviceExtensions.size()) },
			.ppEnabledExtensionNames { deviceExtensions.data() }
		};

		auto [result, dev] = physicalDevice_.createDevice(createInfo);
		if (result != vk::Result::eSuccess)
		{
			return result;
		}

		device_ = std::move(dev);

		graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
		presentQueue_  = device_.getQueue(indices.presentFamily.value(), 0);

		return vk::Result::eSuccess;
	}

	void MainLoop()
	{
		MSG msg{};
		while (GetMessage(&msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
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

	bool IsSupportsRequiredDeviceExtensions(vk::raii::PhysicalDevice physicalDevice)
	{
		auto [res, availableExtensions] = physicalDevice.enumerateDeviceExtensionProperties();

		std::array<const char*, 1> deviceExtensions{ GetRequiredDeviceExtensions() };

		std::unordered_set<std::string_view> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	bool IsRequiredVulkanExtensionsSupported(const std::vector<const char*>& requiredExtensions)
	{
		auto [result, extProps] = vk::enumerateInstanceExtensionProperties();

		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to enumerate Vulkan instance extension properties: {}", vk::to_string(result));
			return false;
		}

		GRV_LOG_TRACE(GRV_CHANNEL(System), "Found {} available Vulkan extensions(s):", extProps.size());

		std::unordered_set<std::string_view> availableExtensions;
		availableExtensions.reserve(extProps.size());

		for (const auto& ext : extProps)
		{
			size_t len = strnlen(ext.extensionName, vk::MaxExtensionNameSize);
			std::string_view sv{ ext.extensionName, len };
			GRV_LOG_TRACE(GRV_CHANNEL(System), "    {}", sv);
			availableExtensions.emplace(sv);
		}

		std::vector<std::string_view> missing;
		for (const char* required : requiredExtensions)
		{
			if (!availableExtensions.contains(required))
			{
				missing.emplace_back(required);
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Required Vulkan extension '{}' is not available.", required);
			}
		}

		if (!missing.empty())
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Missing {} required Vulkan extension(s).", missing.size());
			return false;
		}

		return true;
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

	std::vector<const char*> GetRequiredVulkanExtensions() const
	{
		std::vector<const char*> extensions
		{
			vk::KHRSurfaceExtensionName,
			vk::KHRWin32SurfaceExtensionName
		};

		if (enableValidationLayers_)
		{
			extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	constexpr std::array<const char*, 1> GetRequiredDeviceExtensions()
	{
		return
		{
			vk::KHRSwapchainExtensionName
		};
	}

	constexpr std::array<const char*, 1> GetValidationLayers()
	{
		return 
		{ 
			"VK_LAYER_KHRONOS_validation" 
		};
	}

	template <size_t N>
	bool IsValidationLayersSupported(const std::array<const char*, N>& validationLayers)
	{
		auto [result, availableLayers] = vk::enumerateInstanceLayerProperties();
		if (result != vk::Result::eSuccess)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to enumerate Vulkan Instance Layer Properties: {}", vk::to_string(result));
			return false;
		}

		for (const char* layerName : validationLayers)
		{
			bool layerFound{ false };

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				GRV_LOG_ERROR(GRV_CHANNEL(System), "Vulkan validation layer '{}' is not available.", layerName);
				return false;
			}
		}

		return true;
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
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
