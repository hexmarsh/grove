#pragma once

#define GRV_COMPILER_MSVC  0
#define GRV_COMPILER_GCC   0
#define GRV_COMPILER_CLANG 0

#if defined(_MSC_VER)
	#undef  GRV_COMPILER_MSVC
	#define GRV_COMPILER_MSVC  1
#elif defined(__GNUC__)
	#undef  GRV_COMPILER_GCC
	#define GRV_COMPILER_GCC   1
#elif defined(__clang__)
	#undef  GRV_COMPILER_CLANG
	#define GRV_COMPILER_CLANG 1
#else
	#error "Unknown compiler detected"
#endif