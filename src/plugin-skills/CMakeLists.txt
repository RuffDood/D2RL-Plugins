file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS *.cpp *.h)

add_library(plugin-skills SHARED ${PLUGIN_SOURCES})

target_link_libraries(plugin-skills PRIVATE plugin-shared)

set_target_properties(plugin-skills PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
