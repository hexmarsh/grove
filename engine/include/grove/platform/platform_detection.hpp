#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  define GRV_PLATFORM_WINDOWS
#else
#  error "Unknown platform."
#endif