#include "grove/rhi/vulkan/vulkan_context.hpp"
#include "grove/core/core.hpp"

#if GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#else
#	error "Platform not supported"
#endif

#define VP_USE_OBJECT
#include <vulkan/vulkan_profiles.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <array>
#include <vector>

namespace grove
{ 
	namespace
	{
		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* userData
		)
		{
			if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			{
				GRV_LOG_TRACE("Validation: [{}] {}", 
					pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}
			else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			{
				GRV_LOG_INFO("Validation: [{}] {}", 
					pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}
			else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				GRV_LOG_WARN("Validation: [{}] {}", 
					pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}
			else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				GRV_LOG_ERROR("Validation: [{}] {}", 
					pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}

			return VK_FALSE;
		}
	}

	Error VulkanContext::Init()
	{
		Error err = CreateInstance();
		GRV_ERR_IF(err != Ok, err);

		return Ok;
	}

	void VulkanContext::Shutdown()
	{
		if (surface_ != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}

		if (debugMessenger_ != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
			debugMessenger_ = VK_NULL_HANDLE;
		}

		if (instance_ != VK_NULL_HANDLE)
		{
			vkDestroyInstance(instance_, nullptr);
			instance_ = VK_NULL_HANDLE;
		}
	}

	Error VulkanContext::CreateInstance()
	{
		VkResult result = volkInitialize();
		GRV_ERR_IF(result != VK_SUCCESS, Failed);

		const VpProfileProperties profileProperties
		{
			VP_KHR_ROADMAP_2022_NAME,
			VP_KHR_ROADMAP_2022_SPEC_VERSION
		};

		VpCapabilitiesCreateInfo vpCapsCreateInfo
		{
			.flags = VP_PROFILE_CREATE_STATIC_BIT,
			.apiVersion = VK_API_VERSION_1_1,
			.pVulkanFunctions = nullptr
		};

		VpCapabilities capabilities = VK_NULL_HANDLE;
		result = vpCreateCapabilities(&vpCapsCreateInfo, nullptr, &capabilities);
		GRV_ERR_IF_MSG(result != VK_SUCCESS, CantCreate, "vk.vpCreateCapabilities.failed result={}", string_VkResult(result));

		VkBool32 profileSupported = VK_FALSE;
		result = vpGetInstanceProfileSupport(capabilities, nullptr, &profileProperties, &profileSupported);
		GRV_ERR_IF_MSG(result != VK_SUCCESS && !profileSupported, CantCreate, "Vulkan profile '{}' not supported", VP_KHR_ROADMAP_2022_NAME);

		std::vector<const char*> validationLayers;
#if !defined(NDEBUG)
		validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

		std::vector<const char*> instanceExtensions = 
		{
			VK_KHR_SURFACE_EXTENSION_NAME,
#if GRV_PLATFORM_WINDOWS
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#if !defined(NDEBUG)
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		};

		VkApplicationInfo vkAppInfo
		{ 
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.apiVersion = VP_KHR_ROADMAP_2022_MIN_API_VERSION
		};

		VkInstanceCreateInfo vkCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &vkAppInfo
		};

		VpInstanceCreateInfo vpCreateInfo
		{
			.pCreateInfo = &vkCreateInfo,
			.enabledFullProfileCount = 1,
			.pEnabledFullProfiles = &profileProperties
		};

		result = vpCreateInstance(capabilities, &vpCreateInfo, nullptr, &instance_);
		GRV_ERR_IF_MSG(result != VK_SUCCESS, CantCreate, "vk.createInstance.failed result={}", string_VkResult(result));
		volkLoadInstance(instance_);
		
		GRV_LOG_INFO("Vulkan instance created with profile: {}", 
			VP_KHR_ROADMAP_2022_NAME);

		return Ok;
	}

	Error VulkanContext::CreateSurface(Window& window)
	{
#if GRV_PLATFORM_WINDOWS
		auto& win32Window = static_cast<Win32Window&>(window);

		VkWin32SurfaceCreateInfoKHR createInfo
		{
			.hinstance = win32Window.GetHInstance(),
			.hwnd      = win32Window.GetHWND()
		};

		VkResult result = vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_);
		GRV_ERR_IF_MSG(result != VK_SUCCESS, CantCreate, "vk.createWin32Surface.failed result={}", string_VkResult(result));

		GRV_LOG_INFO("vk.win32Surface.created");
		return Ok;
#else
	#error "Platform not supported"
#endif
	}

	Error VulkanContext::SetupDebugMessenger()
	{
		VkDebugUtilsMessageSeverityFlagsEXT severityFlags =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

		VkDebugUtilsMessageTypeFlagsEXT messageTypeFlags =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debugInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = severityFlags,
			.messageType     = messageTypeFlags,
			.pfnUserCallback = &DebugCallback
		};

		auto result = vkCreateDebugUtilsMessengerEXT(instance_, &debugInfo, nullptr, &debugMessenger_);
		GRV_ERR_IF_MSG(result != VK_SUCCESS, CantCreate, "vk.CreateDebugMessenger.failed result={}", string_VkResult(result));

		GRV_LOG_INFO("vk.debugMessenger.created");
		return Ok;
	}
}
