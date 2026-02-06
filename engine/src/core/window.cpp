#include "grove/core/config.hpp"
#include "grove/core/window.hpp"

#if GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#endif

namespace grove
{
	Result<std::unique_ptr<Window>> Window::Create(const WindowCreateInfo& createInfo)
	{
#if GRV_PLATFORM_WINDOWS
		auto window = std::make_unique<Win32Window>();

		if (Error err = window->Init(createInfo); err != Ok)
		{
			return std::unexpected(err);
		}

		return window;
#else
		GRV_ASSERT(false, "Unknown Platform");
		return nullptr;
#endif
	}
}