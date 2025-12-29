#pragma once

#include <windows.h>

#include "grove/core/window.hpp"

namespace grove
{
	class Win32Window : public Window
	{
	public:
		Win32Window(const WindowCreateInfo &createInfo);
		~Win32Window() override;

		void *GetNativeHandle() const override;

	private:
		void Init(const WindowCreateInfo &createInfo);
		void Shutdown();

		LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) const;
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		HWND hwnd_ = nullptr;
		HINSTANCE hinstance_ = nullptr;
	};
}