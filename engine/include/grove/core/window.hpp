#pragma once

#include <string>

#include "grove/core/typedefs.hpp"
#include "grove/core/memory/box_ptr.hpp"

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

		static BoxPtr<Window> Create(const WindowCreateInfo &create_info);

		virtual void *GetNativeHandle() const = 0;
	};
}