#pragma once

#include "grove/platform/platform_detection.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>

#include <cstddef>
#include <cstdint>

#ifdef GRV_PLATFORM_WINDOWS
#  include <windows.h>
#endif

