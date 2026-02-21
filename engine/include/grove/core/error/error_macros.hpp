#pragma once
#include "grove/core/logging.hpp"
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

#define GRV_ERR_IF(cond, error_enum) \
	do \
	{ \
		if (cond) [[unlikely]] \
		{ \
			GRV_LOG_ERROR_FUNC("Condition '{}' is true.", #cond); \
			return std::unexpected{ ::grove::Error::error_enum }; \
		} \
	} while (0)

#define GRV_ERR_IF_MSG(cond, error_enum, fmt, ...) \
	do \
	{ \
		if (cond) [[unlikely]] \
		{ \
			GRV_LOG_ERROR_FUNC("Condition '{}' is true. " fmt, #cond __VA_OPT__(,) __VA_ARGS__); \
			return std::unexpected{ ::grove::Error::error_enum }; \
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

#define GRV_TRY(expr) \
	do \
	{ \
		auto&& __res = (expr); \
		if (!__res) [[unlikely]] \
		{ \
			return std::unexpected(__res.error()); \
		} \
	} while (0)

#define GRV_TRY_ELSE(expr, elseExpr) \
	do \
	{ \
		auto&& __res = (expr); \
		if (!__res) [[unlikely]] \
		{ \
			elseExpr; \
			return std::unexpected(__res.error()); \
		} \
	} while (0)

#define GRV_TRY_ASSIGN(lhs, rhs) \
	do \
	{ \
		auto&& __res = (rhs); \
		if (!__res) [[unlikely]] \
		{ \
			return std::unexpected(__res.error()); \
		} \
		lhs = std::move(*__res); \
	} while (0)

#define GRV_OK {}

