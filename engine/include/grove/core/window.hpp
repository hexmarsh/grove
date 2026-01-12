#pragma once

#include <string>

#include "grove/core/typedefs.hpp"
#include <memory>

namespace grove
{
	struct WindowCreateInfo
	{
		std::string title;
		u32 width, height;

		bool enableDebugConsole = false;
	};

	class Window
	{
	public:
		virtual ~Window() = default;

		virtual void OnUpdate() = 0;
		virtual u32 GetWidth() const = 0;
		virtual u32 GetHeight() const = 0;


		static std::unique_ptr<Window> Create(const WindowCreateInfo &create_info);

		virtual void* GetNativeHandle() const = 0;
	};
}