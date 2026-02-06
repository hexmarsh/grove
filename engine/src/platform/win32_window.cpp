#include "grove/core/core.hpp"
#include "grove/platform/win32_window.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#if GRV_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace grove
{
	Win32Window::Win32Window()
	{}

	Win32Window::~Win32Window()
	{
		Shutdown();
	}

	void Win32Window::OnUpdate()
	{
		glfwPollEvents();
	}

	u32 Win32Window::GetWidth() const
	{
		int width;
		glfwGetFramebufferSize(window_, &width, nullptr);
		return static_cast<u32>(width);
	}

	u32 Win32Window::GetHeight() const
	{
		int height;
		glfwGetFramebufferSize(window_, nullptr, &height);
		return static_cast<u32>(height);
	}

	void* Win32Window::GetNativeHandle() const
	{
		return window_;
	}

	HINSTANCE Win32Window::GetHInstance() const
	{
		GRV_ASSERT(!window_);
		return GetModuleHandle(nullptr);
	}

	HWND Win32Window::GetHWND() const
	{
		GRV_ASSERT(!window_);
		return glfwGetWin32Window(window_);
	}

	Error Win32Window::Init(const WindowCreateInfo& createInfo)
	{
		GRV_LOG_INFO("win32window.creating width='{}', height='{}'", createInfo.width, createInfo.height);

		GRV_ERR_IF_MSG(!glfwInit(), CantCreate, "Failed to initialize GLFW");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window_ = glfwCreateWindow(
			createInfo.width,
			createInfo.height,
			createInfo.title.c_str(),
			nullptr,
			nullptr
		);
		GRV_ERR_IF_MSG(!window_, CantCreate, "Failed to create GLFW Window");

		return Ok;
	}

	void Win32Window::Shutdown()
	{
		if (window_)
		{
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}

		glfwTerminate();
	}
}