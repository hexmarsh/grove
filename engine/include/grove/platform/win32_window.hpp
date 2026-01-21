#pragma once
#include "grove/core/window.hpp"

#define USE_GLFW  1
#define USE_WIN32 0

#if USE_GLFW
#include <GLFW/glfw3.h>
#endif

#include <windows.h>

namespace grove
{
#if USE_GLFW
	class Win32Window : public Window
	{
	public:
		Win32Window(const WindowCreateInfo& createInfo);
		~Win32Window() override;

		void OnUpdate() override;

		u32 GetWidth() const override;
		u32 GetHeight() const override;

		void* GetNativeHandle() const override;

		HINSTANCE GetHInstance() const;
		HWND GetHWND() const;

	private:
		bool Init(const WindowCreateInfo& createInfo);
		bool Shutdown();

	private:
		GLFWwindow* window_{ nullptr };
	};
#endif // USE_GLFW

// Win32 api - Maybe continued later
#if USE_WIN32
	class Win32Window : public Window
	{
	public:
		Win32Window(const WindowCreateInfo &createInfo);
		~Win32Window() override;
	
		void *GetNativeHandle() const override;
	
		HINSTANCE GetHInstance() const { return hinstance_; }
		HWND GetHWND() const { return hwnd_; }
	
	private:
		void Init(const WindowCreateInfo &createInfo);
		void Shutdown();
	
		LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) const;
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	
		HWND hwnd_ = nullptr;
		HINSTANCE hinstance_ = nullptr;
	};
#endif // USE_WIN32
}