#pragma once

#include "grove/core/config.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>

#include <cstddef>
#include <cstdint>

#if GRV_PLATFORM_WINDOWS
	#include <windows.h>
#endif

