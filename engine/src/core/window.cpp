#include "grove/core/config.hpp"
#include "grove/core/window.hpp"

#include "grove/platform/glfw_window.hpp"
#include "grove/core/memory/box_ptr.hpp"

namespace grove
{
	BoxPtr<Window> Window::Create(const WindowCreateInfo& createInfo)
	{
#if GRV_PLATFORM_WINDOWS
		return BoxPtr<GLFWWindow>::Create(createInfo);
#else
		GRV_ASSERT(false, "Unknown Platform");
		return BoxPtr<Window>{};
#endif
	}
}