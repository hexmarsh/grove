#include <cstdlib>
#include <map>
#include <optional>
#include <string.h>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <windows.h>

#include "grove/core/assert.hpp"
#include "grove/core/grove_engine.hpp"
#include "grove/core/logging/log_macros.hpp"
#include "grove/core/memory/box_ptr.hpp"
#include "grove/core/typedefs.hpp"
#include "grove/core/window.hpp"

#define KiB(x) ((x) >> 10)
#define MiB(x) ((x) >> 20)
#define GiB(x) ((x) >> 30)

VkResult vkCreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
	const VkAllocationCallbacks* allocator,
	VkDebugUtilsMessengerEXT* debugMessenger
)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func == nullptr)
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	return func(instance, createInfo, allocator, debugMessenger);
}

void  vkDestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* allocator
)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (func != nullptr)
	{
		func(instance, debugMessenger, allocator);
	}
}

struct QueueFamilyIndices
{
	std::optional<grove::u32> graphicsFamily{ std::nullopt };

	bool IsComplete() const { return graphicsFamily.has_value(); }
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
		grove = grove::BoxPtr<grove::GroveEngine>::Create();
		grove->Init();

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
			.enable_debug_console { true }
		};

		window = grove::Window::Create(windowCreateInfo);
	}

	bool InitVulkan()
	{
		if (!CreateInstance())
		{
			return false;
		}

		if (!SetupDebugMessenger())
		{
			return false;
		}

		CreateSurface();

		if (!PickPhysicalDevice())
		{
			return false;
		}

		if (!CreateLogicalDevice())
		{
			return false;
		}

		return true;
	}

	bool CreateInstance()
	{
		std::vector<const char*> validationLayers{ GetValidationLayers() };
		if (!CheckValidationLayerSupport(validationLayers))
		{
			return false;
		}

		std::vector<const char*> extensions{ GetRequiredVulkanExtensions() };
		if (!VerifyVulkanExtensions(extensions))
		{
			return false;
		}

		VkApplicationInfo appInfo
		{
			.sType              { VK_STRUCTURE_TYPE_APPLICATION_INFO },
			.pApplicationName   { "Hello Triangle" },
			.applicationVersion { VK_MAKE_VERSION(1, 0, 0) },
			.pEngineName        { "GroveEngine" },
			.engineVersion      { VK_MAKE_VERSION(1, 0, 0) },
			.apiVersion         { VK_API_VERSION_1_0 }
		};

		VkInstanceCreateInfo createInfo
		{
			.sType                   { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO },
			.pApplicationInfo        { &appInfo },
			.enabledExtensionCount   { static_cast<grove::u32>(extensions.size()) },
			.ppEnabledExtensionNames { extensions.data() }
		};

		if (enableValidationLayers_)
		{
			createInfo.enabledLayerCount = validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();

			VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = GetDebugUtilsCreateInfo();
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugUtilsCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult result{ vkCreateInstance(&createInfo, nullptr, &instance) };
		if (result != VK_SUCCESS)
		{
			string_VkResult(result);
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to create Vulkan instance: {}", string_VkResult(result));
			return false;
		}

		return true;
	}

	bool SetupDebugMessenger()
	{
		if (!enableValidationLayers_)
		{
			return true;
		}

		VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo{ GetDebugUtilsCreateInfo() };

		VkResult result{ vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &debugMessenger_) };
		if (result != VK_SUCCESS)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to create Vulkan Debug Messenger: {}", string_VkResult(result));
			return false;
		}

		return true;
	}

	bool CreateSurface()
	{
		return true;
	}

	bool PickPhysicalDevice()
	{
		grove::u32 deviceCount{ 0 };
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to find GPUs with Vulkan support.");
			return false;
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		std::multimap<grove::u32, VkPhysicalDevice> candidates;
		for (const auto& device : devices)
		{
			grove::u32 score{ RateDeviceSuitability(device) };
			candidates.insert(std::make_pair(score, device));
		}

		if (candidates.rbegin()->first <= 0)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to find a suitable GPU for Vulkan.");
			return false;
		}

		physicalDevice_ = candidates.rbegin()->second;
		return true;
	}

	bool CreateLogicalDevice()
	{
		std::vector<const char*> validationLayers{ GetValidationLayers() };
		QueueFamilyIndices indices{ FindQueueFamilies(physicalDevice_) };

		if (!indices.IsComplete())
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to find required queue families.");
			return false;
		}

		grove::f32 queuePriority{ 1.0f };

		VkDeviceQueueCreateInfo queueCreateInfo
		{
			.sType            { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO },
			.queueFamilyIndex { indices.graphicsFamily.value() },
			.queueCount       { 1 },
			.pQueuePriorities { &queuePriority }
		};

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo
		{
			.sType                 { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO },
			.queueCreateInfoCount  { 1 },
			.pQueueCreateInfos     { &queueCreateInfo },
			.enabledExtensionCount { 0 },
			.pEnabledFeatures      { &deviceFeatures }
		};

		if (enableValidationLayers_)
		{
			createInfo.enabledLayerCount = validationLayers.size();
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to create logical device.");
			return false;
		}

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue_);

		return true;
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
		if (device != VK_NULL_HANDLE)
		{
			vkDestroyDevice(device, nullptr);
		}

		if (enableValidationLayers_ && debugMessenger_ != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger_, nullptr);
		}

		if (instance != VK_NULL_HANDLE)
		{
			vkDestroyInstance(instance, nullptr);
		}

		window.Reset();

		if (grove)
		{
			grove->Shutdown();
			grove.Reset();
		}
	}

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices{};

		grove::u32 queueFamilyCount{ 0 };
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (grove::u32 i = 0; i < queueFamilies.size(); ++i)
		{
			if (indices.IsComplete())
			{
				break;
			}

			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}
		}

		return indices;
	}

	grove::u32 RateDeviceSuitability(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProps{};
		vkGetPhysicalDeviceProperties(device, &deviceProps);

		VkPhysicalDeviceFeatures deviceFeats{};
		vkGetPhysicalDeviceFeatures(device, &deviceFeats);

		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(device, &memProps);

		QueueFamilyIndices indices = FindQueueFamilies(device);

		if (!indices.IsComplete())
		{
			return 0;
		}

		if (!deviceFeats.geometryShader)
		{
			return 0;
		}

		grove::u64 score{ 0 };

		// prefer discrete gpu
		if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		// prefer highest vram
		VkDeviceSize vramBytes{ 0 };
		for (size_t i = 0; i < memProps.memoryHeapCount; ++i)
		{
			if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				vramBytes += memProps.memoryHeaps[i].size;
			}
		}

		score += GiB(vramBytes);
		score += deviceProps.limits.maxImageDimension2D;

		return score;
	}

	VkDebugUtilsMessengerCreateInfoEXT GetDebugUtilsCreateInfo()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo
		{
			.sType			 { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT },

			.messageSeverity { VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT },

			.messageType	 { VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
							   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
			                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT },

			.pfnUserCallback { DebugUtilsMessengerCallback }
		};

		return debugUtilsCreateInfo;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* user_data
	)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		{
			GRV_LOG_TRACE(GRV_CHANNEL(System), "{} - {}: {}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			GRV_LOG_INFO(GRV_CHANNEL(System), "{} - {}: {}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			GRV_LOG_WARN(GRV_CHANNEL(System), "{} - {}: {}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "{} - {}: {}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
		}

		return VK_FALSE;
	}

	std::vector<const char*> GetRequiredVulkanExtensions() const
	{
		std::vector<const char*> extensions
		{
			"VK_KHR_surface",
			"VK_KHR_win32_surface"
		};

		if (enableValidationLayers_)
		{
			extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	bool VerifyVulkanExtensions(const std::vector<const char*>& requiredExtensions)
	{
		grove::u32 extensionCount{ 0 };
		VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		if (res != VK_SUCCESS)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to enumerate Vulkan instance extension properties: {}", static_cast<int>(res));
			return false;
		}

		std::vector<VkExtensionProperties> extensions(extensionCount);
		res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
		if (res != VK_SUCCESS)
		{
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Failed to enumerate Vulkan instance extension properties: {}", static_cast<int>(res));
			return false;
		}

		GRV_LOG_TRACE(GRV_CHANNEL(System), "Found {} available Vulkan extensions(s):", extensionCount);

		std::unordered_set<std::string_view> availableExtensions;
		availableExtensions.reserve(extensions.size());

		for (const auto& ext : extensions)
		{
			GRV_LOG_TRACE(GRV_CHANNEL(System), "    {}", ext.extensionName);
			availableExtensions.emplace(ext.extensionName);
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
			GRV_LOG_FATAL(GRV_CHANNEL(System), "Missing {} required Vulkan extension(s).", missing.size());
			return false;
		}

		return true;
	}

	std::vector<const char*> GetValidationLayers()
	{
		return std::vector<const char*>{ "VK_LAYER_KHRONOS_validation" };
	}

	bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers)
	{
		grove::u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound{ false };

			for (const auto& layer_properties : availableLayers)
			{
				if (strcmp(layerName, layer_properties.layerName) == 0)
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
	grove::BoxPtr<grove::GroveEngine> grove;
	grove::BoxPtr<grove::Window>      window;
	VkInstance                        instance                 { VK_NULL_HANDLE };
	VkDebugUtilsMessengerEXT          debugMessenger_          { VK_NULL_HANDLE };
	VkSurfaceKHR                      surface                  { VK_NULL_HANDLE };
	VkPhysicalDevice                  physicalDevice_          { VK_NULL_HANDLE };
	VkDevice                          device                   { VK_NULL_HANDLE };
	VkQueue                           graphicsQueue_           { VK_NULL_HANDLE };
	bool                              enableValidationLayers_ { true };
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
