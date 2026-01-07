#pragma once
#include "grove/core/config.hpp"
#include "grove/core/window.hpp"
#include <GLFW/glfw3.h>

#if GRV_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace grove
{
	class GLFWWindow : public Window
	{
	public:
		GLFWWindow(const WindowCreateInfo& createInfo);
		~GLFWWindow() override;

		void OnUpdate() override;

		void* GetNativeHandle() const override;

#if GRV_PLATFORM_WINDOWS
		HINSTANCE GetHInstance() const;
		HWND GetHWND() const;
#endif

	private:
		bool Init(const WindowCreateInfo& createInfo);
		bool Shutdown();

	private:
		GLFWwindow* window_{ nullptr };
	};
}