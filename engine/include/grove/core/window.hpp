#pragma once
#include "grove/core/typedefs.hpp"
#include "grove/core/error/error.hpp"
#include <memory>
#include <string>

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

		static Result<std::unique_ptr<Window>> Create(const WindowCreateInfo &create_info);

		virtual void* GetNativeHandle() const = 0;
	};
}