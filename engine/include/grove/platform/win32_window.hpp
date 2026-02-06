#pragma once
#include "grove/core/window.hpp"

#include <GLFW/glfw3.h>

struct HWND__;
using HWND = HWND__*;

struct HINSTANCE__;
using HINSTANCE = HINSTANCE__*;

namespace grove
{
	enum class Error;

	class Win32Window : public Window
	{
	public:
		Win32Window();
		~Win32Window() override;

		void OnUpdate() override;

		u32 GetWidth() const override;
		u32 GetHeight() const override;

		void* GetNativeHandle() const override;

		HINSTANCE GetHInstance() const;
		HWND GetHWND() const;

		Error Init(const WindowCreateInfo& createInfo);
	private:
		void Shutdown();

	private:
		GLFWwindow* window_{ nullptr };
	};
}