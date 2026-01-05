#pragma once

#define GRV_PLATFORM_WINDOWS 0

#if defined(_WIN32)
	#undef  GRV_PLATFORM_WINDOWS
	#define GRV_PLATFORM_WINDOWS 1
#else
	#error Unsupported platform
#endif