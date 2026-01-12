add_library(grove_engine)

target_sources(grove_engine
	PRIVATE
		src/core/logging/logger.cpp
		src/core/window.cpp 
		src/core/grove_engine.cpp
		src/platform/glfw_window.cpp
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

find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(grove_engine
	PUBLIC
		glfw
		glm::glm
		Vulkan::Vulkan
)
