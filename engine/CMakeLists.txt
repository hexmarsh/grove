add_library(grove_engine)

find_package(Vulkan REQUIRED)

target_sources(grove_engine
	PRIVATE
		src/core/logging/logger.cpp
		src/core/window.cpp 
		src/platform/win32_window.cpp 
		src/core/grove_engine.cpp)

target_include_directories(grove_engine
	PUBLIC
		${PROJECT_SOURCE_DIR}/engine/include
	PRIVATE
		${PROJECT_SOURCE_DIR}/engine/src
)

target_precompile_headers(grove_engine
	PRIVATE
		src/grvpch.hpp
)

if(WIN32)
	target_compile_definitions(grove_engine
		PRIVATE
			WIN32_LEAN_AND_MEAN
			NOMINMAX
	)

	target_compile_options(grove_engine PUBLIC "/Zc:preprocessor")
endif()

target_link_libraries(grove_engine
	PUBLIC
		Vulkan::Vulkan
)
