#include "grove/rhi/vulkan/vk_context.hpp"

#if GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#else
#	error "Platform not supported"
#endif

#include <vulkan/vulkan_to_string.hpp>
#include <array>
#include <vector>

namespace grove
{
	namespace
	{
		static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
			vk::DebugUtilsMessageTypeFlagsEXT type,
			const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* userData
		)
		{
			if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
			{
				GRV_LOG_TRACE("event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}
			else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
			{
				GRV_LOG_INFO("event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}
			else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
			{
				GRV_LOG_WARN("event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}
			else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
			{
				GRV_LOG_ERROR("event=vk.validation msgId={} msgIdName=\"{}\" msg=\"{}\"", pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			}

			return vk::False;
		}
	}

	Result<VKContext> VKContext::Create(const Window& window)
	{
		VKContext context;

		GRV_TRY(context.CreateInstance());
		GRV_TRY(context.SetupDebugMessenger());
		GRV_TRY(context.CreateSurface(window));

		return context;
	}

	const vk::raii::Instance& VKContext::GetInstance() const
	{
		return instance_;
	}

	const vk::raii::SurfaceKHR& VKContext::GetSurface() const
	{
		return surface_;
	}

	Status VKContext::CreateInstance()
	{
		constexpr std::array requiredLayers
		{
			"VK_LAYER_KHRONOS_validation"
		};

		constexpr std::array requiredExtensions
		{
			vk::EXTDebugUtilsExtensionName,
			vk::KHRSurfaceExtensionName,
#if GRV_PLATFORM_WINDOWS
			vk::KHRWin32SurfaceExtensionName
#endif
		};

		GRV_TRY(AreLayersSupported(requiredLayers));
		GRV_TRY(AreExtensionsSupported(requiredExtensions));

		VpCapabilitiesCreateInfo vpCapsCreateInfo
		{
			.flags = VP_PROFILE_CREATE_STATIC_BIT,
			.apiVersion = VK_API_VERSION_1_1,
			.pVulkanFunctions = nullptr
		};

		VpCapabilities capabilities = VK_NULL_HANDLE;
		vpCreateCapabilities(&vpCapsCreateInfo, nullptr, &capabilities);

		constexpr VpProfileProperties profileProperties = GetVPProfileProperties();

		vk::Bool32 profileSupported = vk::False;
		if (vk::Result result = vk::Result(vpGetInstanceProfileSupport(capabilities, nullptr, &profileProperties, &profileSupported)); result != vk::Result::eSuccess)
		{
			GRV_ERR_IF_MSG(true, CantCreate, "vp.getInstanceProfileSupport failed result={}", vk::to_string(result));
		}
		GRV_ERR_IF_MSG(profileSupported != vk::True, CantCreate, "Vulkan Profile '{}' is not supported.", profileProperties.profileName);

		vk::ApplicationInfo vkAppInfo
		{
			.apiVersion = VP_KHR_ROADMAP_2024_MIN_API_VERSION
		};

		vk::InstanceCreateInfo vkCreateInfo{};
		vkCreateInfo
			.setPApplicationInfo(&vkAppInfo)
			.setPEnabledLayerNames(requiredLayers)
			.setPEnabledExtensionNames(requiredExtensions);

		VpInstanceCreateInfo vpCreateInfo
		{
			.pCreateInfo = &*vkCreateInfo,
			.enabledFullProfileCount = 1,
			.pEnabledFullProfiles = &profileProperties
		};

		VkInstance rawInstance = VK_NULL_HANDLE;
		if (vk::Result result = vk::Result(vpCreateInstance(capabilities, &vpCreateInfo, nullptr, &rawInstance)); result != vk::Result::eSuccess)
		{
			GRV_ERR_IF_MSG(true, CantCreate, "vpCreateInstance failed. result={}", vk::to_string(result));
		}

		instance_ = vk::raii::Instance(context_, rawInstance);

		return GRV_OK;
	}

	Status VKContext::CreateSurface(const Window& window)
	{
#if GRV_PLATFORM_WINDOWS
		const auto& win32Window = dynamic_cast<const Win32Window&>(window);

		vk::Win32SurfaceCreateInfoKHR createInfo
		{
			.hinstance = win32Window.GetHInstance(),
			.hwnd = win32Window.GetHWND()
		};

		auto surface = instance_.createWin32SurfaceKHR(createInfo);
		GRV_ERR_IF_MSG(!surface, CantCreate, "vk.createWin32Surface.failed result={}", vk::to_string(surface.error()));

		surface_ = std::move(*surface);
		GRV_LOG_INFO("vk.win32Surface.created");

		return GRV_OK;
#else
#error "Platform not supported"
#endif
	}

	Status VKContext::SetupDebugMessenger()
	{
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags
		{
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		};

		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags
		{
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		};

		vk::DebugUtilsMessengerCreateInfoEXT debugInfo
		{
			.messageSeverity { severityFlags },
			.messageType     { messageTypeFlags },
			.pfnUserCallback { &DebugCallback }
		};

		auto debugUtilsMessenger = instance_.createDebugUtilsMessengerEXT(debugInfo, nullptr);
		GRV_ERR_IF_MSG(!debugUtilsMessenger, CantCreate, "vk.CreateDebugMessenger.failed result={}",
			vk::to_string(debugUtilsMessenger.error()));
		debugMessenger_ = std::move(*debugUtilsMessenger);

		GRV_LOG_INFO("vk.debugMessenger.created");
		return GRV_OK;
	}

	Status VKContext::AreLayersSupported(std::span<const char* const> requiredLayers) const
	{
		std::vector<vk::LayerProperties> availableLayers = context_.enumerateInstanceLayerProperties().value();

		bool anyLayerNotFound{ false };
		for (std::string_view layerName : requiredLayers)
		{
			bool layerFound{ false };
			for (const auto& layerProp : availableLayers)
			{
				if (std::strcmp(layerName.data(), layerProp.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				anyLayerNotFound = true;
			}

			GRV_WARN_IF(!layerFound, "Required Vulkan layer '{}' is not available.", layerName);
		}

		GRV_ERR_IF_MSG(anyLayerNotFound, NotFound,
			"One or more required Vulkan layers were not found.\n"
			"Make sure the Vulkan SDK is installed correctly.");

		return GRV_OK;
	}

	Status VKContext::AreExtensionsSupported(const std::span<const char* const> requiredExtensions) const
	{
		std::vector<vk::ExtensionProperties> availableExtensions = context_.enumerateInstanceExtensionProperties().value();

		bool anyExtensionsNotFound{ false };
		for (std::string_view extensionName : requiredExtensions)
		{
			bool extensionFound{ false };
			for (const auto& extensionProp : availableExtensions)
			{
				if (std::strcmp(extensionName.data(), extensionProp.extensionName) == 0)
				{
					extensionFound = true;
					break;
				}
			}

			if (!extensionFound)
			{
				anyExtensionsNotFound = true;
				GRV_LOG_WARN("Required Vulkan extension '{}' is not available.", extensionName);
			}
		}

		GRV_ERR_IF_MSG(anyExtensionsNotFound, NotFound,
			"One or more required Vulkan extensions were not found.\n"
			"Make sure the Vulkan SDK is installed correctly.");

		return GRV_OK;
	}
}
