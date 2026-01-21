# Library setup
add_library(grove_engine)

target_sources(grove_engine
	PRIVATE
		src/core/logging/logger.cpp
		src/core/window.cpp 
		src/core/grove_engine.cpp
		src/platform/win32_window.cpp
)

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
		PUBLIC
			NOMINMAX
			WIN32_LEAN_AND_MEAN
	)

	target_compile_options(grove_engine
		PUBLIC
			"/Zc:preprocessor"
	)
endif()

# Dependencies
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(grove_engine
	PUBLIC
		glfw
		glm::glm
		Vulkan::Vulkan
)

# Shaders
include(${CMAKE_SOURCE_DIR}/cmake/add_slang_target.cmake)

add_slang_shader_target(spirv_shaders SOURCES
	"${CMAKE_SOURCE_DIR}/engine/assets/shaders/shader.slang"
)

# Copy assets
add_custom_target(copy_assets
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/assets"
		"${CMAKE_CURRENT_BINARY_DIR}/assets"
	COMMENT "Copying assets to build directory"
)

add_dependencies(copy_assets spirv_shaders)
add_dependencies(grove_engine copy_assets)
