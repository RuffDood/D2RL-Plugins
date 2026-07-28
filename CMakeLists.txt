cmake_minimum_required(VERSION 3.24)
project(D2RLPlugins LANGUAGES CXX)

include(CTest)

set(HOOK_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/hook-manifest.json")
set(HOOK_MANIFEST_VALIDATOR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ValidateHookManifest.cmake")

execute_process(
	COMMAND "${CMAKE_COMMAND}" -DHOOK_MANIFEST=${HOOK_MANIFEST} -P ${HOOK_MANIFEST_VALIDATOR}
	RESULT_VARIABLE hook_manifest_result
	OUTPUT_VARIABLE hook_manifest_output
	ERROR_VARIABLE hook_manifest_error
)
if(NOT hook_manifest_result EQUAL 0)
	message(FATAL_ERROR "Hook manifest validation failed:\n${hook_manifest_output}${hook_manifest_error}")
endif()
string(STRIP "${hook_manifest_output}" hook_manifest_output)
message(STATUS "${hook_manifest_output}")

add_custom_target(validate-hook-manifest ALL
	COMMAND "${CMAKE_COMMAND}" -DHOOK_MANIFEST=${HOOK_MANIFEST} -P ${HOOK_MANIFEST_VALIDATOR}
	DEPENDS ${HOOK_MANIFEST} ${HOOK_MANIFEST_VALIDATOR}
	COMMENT "Validating hook ownership and memory-write overlaps"
	VERBATIM
)

add_subdirectory(src)

foreach(plugin_target IN ITEMS plugin-items plugin-levels plugin-misc plugin-quests plugin-skills)
	if(TARGET ${plugin_target})
		add_dependencies(${plugin_target} validate-hook-manifest)
	endif()
endforeach()

if(BUILD_TESTING)
	add_test(
		NAME hook-manifest-valid
		COMMAND "${CMAKE_COMMAND}" -DHOOK_MANIFEST=${HOOK_MANIFEST} -P ${HOOK_MANIFEST_VALIDATOR}
	)
	add_test(
		NAME hook-manifest-overlap-rejected
		COMMAND "${CMAKE_COMMAND}"
			-DVALIDATOR=${HOOK_MANIFEST_VALIDATOR}
			-DINVALID_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/hook-manifest-overlap.json
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AssertInvalidHookManifest.cmake
	)
endif()
