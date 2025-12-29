add_executable(grove_editor
	WIN32
	src/main.cpp
)

target_sources(grove_editor
	PRIVATE
		# imgui
		${PROJECT_SOURCE_DIR}/extern/imgui/imgui.cpp
		#${PROJECT_SOURCE_DIR}/extern/imgui/imgui_demo.cpp
		${PROJECT_SOURCE_DIR}/extern/imgui/imgui_draw.cpp
		${PROJECT_SOURCE_DIR}/extern/imgui/imgui_tables.cpp
		${PROJECT_SOURCE_DIR}/extern/imgui/imgui_widgets.cpp
)

target_include_directories(grove_editor
	PRIVATE
		${PROJECT_SOURCE_DIR}/extern/imgui
		${PROJECT_SOURCE_DIR}/extern/imgui/backends
)

target_link_libraries(grove_editor
	PRIVATE
		grove_engine
)
