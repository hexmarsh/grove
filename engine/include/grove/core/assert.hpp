#pragma once

#include <source_location>
#include <format>
#include "grove/core/config.h"
#include "grove/core/logging.hpp"
#include "grove/core/logging/log_macros.hpp"

#if defined(GRV_ENABLE_ASSERTS)
	#if defined(GRV_COMPILER_MSVC)
		#define GRV_DEBUG_BREAK() __debugbreak()
	#elif defined(GRV_COMPILER_CLANG)
		#define GRV_DEBUG_BREAK() __builtin_debugtrap()
	#elif  defined(GRV_COMPILER_GCC)
		#define GRV_DEBUG_BREAK() __builtin_trap()
	#endif
	
	#define GRV_ASSERT(expr, ...) \
		do \
		{ \
			if (!(expr)) \
			{ \
				const auto location = std::source_location::current(); \
				GRV_LOG_FATAL(\
					GRV_CHANNEL(Assert), \
					"{}" __VA_OPT__(": {}") " ({}:{} {})", \
					#expr __VA_OPT__(, std::format(__VA_ARGS__)), \
					location.file_name(), \
					location.line(), \
					location.function_name() \
				); \
				GRV_DEBUG_BREAK(); \
			} \
		} while (0)
#else
	#define GRV_DEBUG_BREAK()
	#define GRV_ASSERT(expr) ((void)0)
	#define GRV_ASSERT(expr, ...) ((void)0)
#endif
