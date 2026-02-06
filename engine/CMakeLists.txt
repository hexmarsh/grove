cmake_minimum_required(VERSION 3.21)
project(grove_engine LANGUAGES C CXX)

# ============================================================
# Vulkan SDK (LunarG) check
# ============================================================

find_package(Vulkan QUIET)

if (NOT Vulkan_FOUND)
    message(FATAL_ERROR
        "Vulkan SDK (LunarG) was not found.\n"
        "Please install the Vulkan SDK from:\n"
        "  https://vulkan.lunarg.com/\n\n"
        "After installation, make sure CMake can find it "
        "(VULKAN_SDK environment variable is usually set automatically)."
    )
endif()

# ============================================================
# Engine library
# ============================================================

add_library(grove_engine)

# ------------------------------------------------------------
# Engine sources
# ------------------------------------------------------------

set(GROVE_ENGINE_CORE_SOURCES
    src/core/logging.cpp
    src/core/window.cpp
    src/core/application.cpp
    src/core/grove_engine.cpp
)

set(GROVE_ENGINE_PLATFORM_SOURCES
    src/platform/win32_window.cpp
)

set(GROVE_ENGINE_VULKAN_SOURCES
    src/rhi/vulkan/vulkan_context.cpp
)

set(GROVE_ENGINE_VOLK_SOURCE
    $ENV{VULKAN_SDK}/Include/volk/volk.c
)

set_source_files_properties(${GROVE_ENGINE_VOLK_SOURCE} PROPERTIES LANGUAGE CXX)

target_sources(grove_engine
    PRIVATE
        ${GROVE_ENGINE_CORE_SOURCES}
        ${GROVE_ENGINE_PLATFORM_SOURCES}
        ${GROVE_ENGINE_VULKAN_SOURCES}
        ${GROVE_ENGINE_VOLK_SOURCE}
 )

# ------------------------------------------------------------  
# Include paths  
# ------------------------------------------------------------  

target_include_directories(grove_engine  
    PUBLIC  
        ${PROJECT_SOURCE_DIR}/include  
        $ENV{VULKAN_SDK}/Include  
    PRIVATE  
        ${PROJECT_SOURCE_DIR}/src  
)

# ------------------------------------------------------------
# Precompiled headers
# ------------------------------------------------------------

#target_precompile_headers(grove_engine
#    PRIVATE
#        src/grvpch.hpp
#)

# ------------------------------------------------------------
# Platform configuration
# ------------------------------------------------------------

if (WIN32)
    target_compile_definitions(grove_engine
        PUBLIC
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            VK_USE_PLATFORM_WIN32_KHR
    )

    target_compile_options(grove_engine
        PUBLIC
            /Zc:preprocessor
    )
endif()

# ============================================================
# Dependencies
# ============================================================

find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ------------------------------------------------------------
# Link engine dependencies
# ------------------------------------------------------------

target_link_libraries(grove_engine
    PUBLIC
        Vulkan::Vulkan
        glm::glm
        glfw
        spdlog::spdlog
)

# ============================================================
# Shaders
# ============================================================

include(${CMAKE_SOURCE_DIR}/cmake/add_slang_target.cmake)

add_slang_shader_target(spirv_shaders
    SOURCES
        "${CMAKE_SOURCE_DIR}/engine/assets/shaders/shader.slang"
)

# ============================================================
# Assets
# ============================================================

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMENT "Copying assets to build directory"
)

add_dependencies(copy_assets spirv_shaders)
add_dependencies(grove_engine copy_assets)
