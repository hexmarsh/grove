#pragma once

#include <source_location>
#include <format>
#include "grove/core/logging.hpp"
#include "grove/core/logging/log_macros.hpp"

#define GRV_ASSERTIONS_ENABLED
#ifdef GRV_ASSERTIONS_ENABLED

#if defined(_MSC_VER)
	#define GRV_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
	#define GRV_DEBUG_BREAK() __builtin_debugtrap()
#elif  defined(__GNUC__)
	#define GRV_DEBUG_BREAK() __builtin_trap()
#else
	#include <signal.h>
	#if defined(SIGTRAP)
		#define GRV_DEBUG_BREAK() raise(SIGTRAP)
	#else
		#define GRV_DEBUG_BREAK() raise(SIGABRT)
	#endif
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
