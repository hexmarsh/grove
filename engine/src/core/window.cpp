#include "grove/core/window.hpp"

#include "grove/platform/platform_detection.hpp"

#ifdef GRV_PLATFORM_WINDOWS
#include "grove/platform/win32_window.hpp"
#endif

#include "grove/core/memory/box_ptr.hpp"
#include "grove/core/assert.hpp"

namespace grove
{
	BoxPtr<Window> Window::Create(const WindowCreateInfo& createInfo)
	{
#ifdef GRV_PLATFORM_WINDOWS
		return BoxPtr<Win32Window>::Create(createInfo);
#else
		GRV_ASSERT(false, "Unknown Platform");
		return BoxPtr<Window>{};
#endif
	}
}