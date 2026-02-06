#pragma once
#include "grove/core/error.hpp"
#include <Volk/volk.h>

namespace grove
{
	class Window;

	class VulkanContext
	{
	public:
		Error Init();
		void Shutdown();

		Error CreateSurface(Window& window);
	
	private:
		Error CreateInstance();
		Error SetupDebugMessenger();

	private:
		VkInstance               instance_       { VK_NULL_HANDLE };
		VkDebugUtilsMessengerEXT debugMessenger_ { VK_NULL_HANDLE };
		VkSurfaceKHR             surface_        { VK_NULL_HANDLE };
		VkDevice                 device_         { VK_NULL_HANDLE };
	};
}