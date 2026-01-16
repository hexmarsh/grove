#include "grove/core/config.hpp"
#include "grove/platform/win32_window.hpp"

#if USE_GLFW
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif // USE_GLFW

#include "grove/core/assert.hpp"
#include "grove/core/logging/log_macros.hpp"

namespace grove
{
#if USE_GLFW
	Win32Window::Win32Window(const WindowCreateInfo& createInfo)
	{
		GRV_LOG_INFO(GRV_CHANNEL(System), "event=window.creating width={} height={}", createInfo.width, createInfo.height);
		Init(createInfo);
	}

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
		return GetModuleHandle(nullptr);
	}

	HWND Win32Window::GetHWND() const
	{
		return glfwGetWin32Window(window_);
	}

	bool Win32Window::Init(const WindowCreateInfo& createInfo)
	{
		if (!glfwInit())
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=glfw.init.failed");
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
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=glfw.window.createFailed title=\"{}\" width={} height={}", createInfo.title, createInfo.width, createInfo.height);
			Shutdown();
			return false;
		}

		return true;
	}

	bool Win32Window::Shutdown()
	{
		if (window_)
		{
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}

		glfwTerminate();
		return true;
	}
#endif // USE_GLFW

#if USE_WIN32
	namespace
	{
		void OpenDebugConsole()
		{
			AllocConsole();

			// redirect io
			FILE* stream;
			freopen_s(&stream, "CONOUT$", "w", stdout);
			freopen_s(&stream, "CONOUT$", "w", stderr);
			freopen_s(&stream, "CONIN$", "r", stdin);

			SetConsoleTitle(TEXT("GroveEngine Debug Console"));
		}

		void CloseDebugConsole()
		{
			FreeConsole();
		}
	}

	Win32Window::Win32Window(const WindowCreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Win32Window::~Win32Window()
	{
		Shutdown();
	}

	void* Win32Window::GetNativeHandle() const
	{
		return static_cast<void*>(hwnd_);
	}

	void Win32Window::Init(const WindowCreateInfo& createInfo)
	{
		if (createInfo.enableDebugConsole)
		{
			OpenDebugConsole();
		}

		GRV_LOG_INFO(
			GRV_CHANNEL(System),
			"event=window.creating title=\"{}\" width={} height={}",
			createInfo.title,
			createInfo.width,
			createInfo.height
		);

		constexpr LPCTSTR CLASS_NAME = TEXT("GroveEngineWindowClass");

		hinstance_ = GetModuleHandle(NULL);

		WNDCLASS wc{
			.lpfnWndProc = WindowProc,
			.hInstance = hinstance_,
			.lpszClassName = CLASS_NAME,
		};

		if (!RegisterClass(&wc))
		{
			GRV_LOG_WARN(GRV_CHANNEL(System), "event=window.registerClass.failed errorCode={}", GetLastError());
		}

		hwnd_ = CreateWindowEx(
			0,
			CLASS_NAME,
			TEXT(createInfo.title.c_str()),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, createInfo.width, createInfo.height,
			nullptr,
			nullptr,
			wc.hInstance,
			this);

		if (!hwnd_)
		{
			GRV_LOG_ERROR(GRV_CHANNEL(System), "event=window.create.failed errorCode={}", GetLastError());
			return;
		}

		ShowWindow(hwnd_, SW_SHOW);
		UpdateWindow(hwnd_);
	}

	void Win32Window::Shutdown()
	{
		if (hwnd_)
		{
			CloseDebugConsole();
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
		}

		if (hinstance_)
		{
			UnregisterClass(TEXT("GroveEngineWindowClass"), hinstance_);
			hinstance_ = nullptr;
		}
	}

	LRESULT Win32Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) const
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd_, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			EndPaint(hwnd_, &ps);
		}
		return 0;

		default:
			return DefWindowProc(hwnd_, uMsg, wParam, lParam);
		}
	}

	LRESULT CALLBACK Win32Window::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		Win32Window* window = nullptr;

		if (uMsg == WM_NCCREATE)
		{
			auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
			window = static_cast<Win32Window*>(create->lpCreateParams);
			SetWindowLongPtr(
				hWnd,
				GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(window));
		}
		else
		{
			window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		}

		if (window)
		{
			window->HandleMessage(uMsg, wParam, lParam);
		}

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
#endif // USE_WIN32
}