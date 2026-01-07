include(FetchContent)

# Vulkan Headers
FetchContent_Declare(
	VulkanHeaders
	GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
	GIT_TAG        450bd2232225d6c7728a4108055ac2e37cef6475 #v1.4.337
)
FetchContent_MakeAvailable(VulkanHeaders)

# Vulkan loader
find_package(Vulkan REQUIRED)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG        7b6aead9fb88b3623e3b3725ebb42670cbe4c579 #3.4
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG        8d1fd52e5ab5590e2c81768ace50c72bae28f2ed #1.0.3
)
FetchContent_MakeAvailable(glm)
