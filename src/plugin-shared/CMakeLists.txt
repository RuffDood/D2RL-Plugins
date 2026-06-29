file(GLOB PLUGIN_SHARED_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)

add_library(plugin-shared SHARED ${PLUGIN_SHARED_SOURCES})

target_compile_features(plugin-shared PUBLIC cxx_std_20)
target_compile_definitions(plugin-shared PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_include_directories(plugin-shared
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PUBLIC ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(plugin-shared PROPERTIES
	OUTPUT_NAME "00pluginpack-shared"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
