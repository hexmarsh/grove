#include "grove/core/config.hpp"
#include "grove/platform/glfw_window.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace grove
{
	GLFWWindow::GLFWWindow(const WindowCreateInfo& createInfo)
	{
		Init(createInfo);
	}

	GLFWWindow::~GLFWWindow()
	{
		Shutdown();
	}

	void GLFWWindow::OnUpdate()
	{
		glfwPollEvents();
	}

	void* GLFWWindow::GetNativeHandle() const
	{
		return window_;
	}

#if GRV_PLATFORM_WINDOWS
	HINSTANCE GLFWWindow::GetHInstance() const
	{
		return GetModuleHandle(nullptr);
	}

	HWND GLFWWindow::GetHWND() const
	{
		return glfwGetWin32Window(window_);
	}
#endif

	bool GLFWWindow::Init(const WindowCreateInfo& createInfo)
	{
		if (!glfwInit())
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "GLFW failed to initialize");
			return false;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window_ = glfwCreateWindow(
			createInfo.width,
			createInfo.height,
			createInfo.title.c_str(),
			nullptr,
			nullptr
		);

		if (!window_)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "Failed to create GLFW window");
			Shutdown();
			return false;
		}

		return true;
	}

	bool GLFWWindow::Shutdown()
	{
		if (window_)
		{
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}

		glfwTerminate();
		return true;
	}
}