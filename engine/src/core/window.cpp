#include "grove/core/config.hpp"
#include "grove/core/window.hpp"
#include "grove/core/error.hpp"

#if GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#endif

namespace grove
{
	Result<std::unique_ptr<Window>> Window::Create(const WindowCreateInfo& createInfo)
	{
#if GRV_PLATFORM_WINDOWS
		auto window = std::make_unique<Win32Window>();

		GRV_TRY(window->Init(createInfo));

		return window;
#else
		GRV_ASSERT(false, "Unknown Platform");
		return nullptr;
#endif
	}
}