set(_vcpkg_installed_dir "${CMAKE_BINARY_DIR}/vcpkg_installed")

find_program(SLANGC_EXECUTABLE
	NAMES slangc slangc.exe
	HINTS "${_vcpkg_installed_dir}/${VCPKG_TARGET_TRIPLET}/tools/shader-slang"
)

if(NOT SLANGC_EXECUTABLE)
	message(FATAL_ERROR "slangc not found. Looked in: ${_vcpkg_installed_dir}/${VCPKG_TARGET_TRIPLET}/tools/shader-slang")
endif()

message(STATUS "slangc: ${SLANGC_EXECUTABLE}")

function(add_slang_shader_target TARGET)
	cmake_parse_arguments(SHADER "" "" "SOURCES" ${ARGN})

	if(NOT SHADER_SOURCES)
		message(FATAL_ERROR "add_slang_shader_target(${TARGET}): no SOURCES provided")
	endif()

	set(SHADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/assets/shaders")
	set(SHADER_OUT "${SHADERS_DIR}/slang.spv")

	add_custom_command(
		OUTPUT "${SHADER_OUT}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADERS_DIR}"

		COMMAND "${SLANGC_EXECUTABLE}"
				${SHADER_SOURCES}
				-target spirv
				-profile spirv_1_4
				-emit-spirv-directly
				-fvk-use-entrypoint-name
				-entry vertMain
				-entry fragMain
				-o "${SHADER_OUT}"

		DEPENDS ${SHADER_SOURCES}
		COMMENT "Compiling Slang Shaders -> SPIR-V"
		VERBATIM
	)

	add_custom_target(${TARGET} DEPENDS "${SHADER_OUT}")
endfunction()