add_executable(grove_editor
	src/main.cpp
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(grove_editor
	PRIVATE
		grove_engine
		imgui::imgui
)
