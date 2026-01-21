#pragma once

#include "grove/core/config.hpp"

#if GRV_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
#endif

#include <algorithm>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>
#include <memory>
#include <source_location>
#include <string>

#include <cstddef>
#include <cstdint>

#if GRV_PLATFORM_WINDOWS
	#include <windows.h>
#endif

