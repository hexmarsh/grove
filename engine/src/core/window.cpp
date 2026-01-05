#include "grove/core/config.h"
#include "grove/core/window.hpp"

#if GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#endif

#include "grove/core/memory/box_ptr.hpp"
#include "grove/core/assert.hpp"

namespace grove
{
	BoxPtr<Window> Window::Create(const WindowCreateInfo& createInfo)
	{
#if GRV_PLATFORM_WINDOWS
		return BoxPtr<Win32Window>::Create(createInfo);
#else
		GRV_ASSERT(false, "Unknown Platform");
		return BoxPtr<Window>{};
#endif
	}
}