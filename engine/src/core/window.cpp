#include "grove/core/config.hpp"
#include "grove/core/window.hpp"

#if GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#endif

namespace grove
{
	std::unique_ptr<Window> Window::Create(const WindowCreateInfo& createInfo)
	{
#if GRV_PLATFORM_WINDOWS
		return std::make_unique<Win32Window>(createInfo);
#else
		GRV_ASSERT(false, "Unknown Platform");
		return nullptr;
#endif
	}
}