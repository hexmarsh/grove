add_executable(grove_editor
	src/newmain.cpp
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(grove_editor
	PRIVATE
		grove_engine
		imgui::imgui
)

# Copy assets from engine
add_custom_target(editor_assets
	COMMAND ${CMAKE_COMMAND} -E make_directory
		"$<TARGET_FILE_DIR:grove_editor>/assets/shaders"

	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/engine/assets"
		"$<TARGET_FILE_DIR:grove_editor>/assets"

	COMMENT "Copying engine assets to editor build directory"
)

add_dependencies(editor_assets copy_assets)
add_dependencies(grove_editor editor_assets)

