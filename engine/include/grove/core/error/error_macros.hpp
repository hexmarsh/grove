#pragma once
#include "grove/core/error.hpp"

#ifndef NDEBUG
	#if defined(GRV_COMPILER_MSVC)
		#define GRV_DEBUG_BREAK() __debugbreak()
	#elif defined(GRV_COMPILER_CLANG)
		#define GRV_DEBUG_BREAK() __builtin_debugtrap()
	#elif defined(GRV_COMPILER_GCC)
		#define GRV_DEBUG_BREAK() __builtin_trap()
	#else
		#define GRV_DEBUG_BREAK() ((void)0)
	#endif
#else
	#define GRV_DEBUG_BREAK() ((void)0)
#endif

#ifndef NDEBUG
	#define GRV_ASSERT(cond) \
	do \
	{ \
		if (!(cond)) [[unlikely]] \
		{ \
			GRV_LOG_FATAL_FUNC("Assert failed '{}' is false.", #cond); \
			GRV_DEBUG_BREAK(); \
		} \
	} while (0)
#else
	#define GRV_ASSERT(cond) ((void)0)
#endif

#define GRV_FATAL_IF(cond)\
	do \
	{ \
		if (cond) [[unlikely]] \
		{ \
			GRV_LOG_FATAL_FUNC("Condition '{}' is true.", #cond); \
			GRV_DEBUG_BREAK(); \
			std::terminate(); \
		} \
	} while (0)

#define GRV_FATAL_IF_MSG(cond, fmt, ...) \
	do \
	{ \
		if (cond) [[unlikely]] \
		{ \
			GRV_LOG_FATAL_FUNC("Condition '{}' is true. " fmt, #cond __VA_OPT__(,) __VA_ARGS__); \
			GRV_DEBUG_BREAK(); \
			std::terminate(); \
		} \
	} while (0)

#define GRV_ERR_IF(cond, ret) \
	do \
	{ \
		if (cond) [[unlikely]] \
		{ \
			GRV_LOG_ERROR_FUNC("Condition '{}' is true.", #cond); \
			return ret; \
		} \
	} while (0)

#define GRV_ERR_IF_MSG(cond, ret, fmt, ...) \
	do \
	{ \
		if (cond) [[unlikely]] \
		{ \
			GRV_LOG_ERROR_FUNC("Condition '{}' is true. " fmt, #cond __VA_OPT__(,) __VA_ARGS__); \
			return ret; \
		} \
	} while (0)

#define GRV_WARN_IF(cond, ...) \
	do \
	{ \
		if (cond) \
		{ \
			GRV_LOG_WARN_FUNC(__VA_ARGS__); \
		} \
	} while (0)

