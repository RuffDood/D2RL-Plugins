include(FetchContent)

FetchContent_Declare(quickjs_ng
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG        v0.9.0
)
# Suppress quickjs-ng's optional build targets
set(BUILD_QJS_LIBC   OFF CACHE BOOL "" FORCE)
set(BUILD_QJS_CLI    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs_ng)

file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS *.cpp *.h)

add_library(plugin-script SHARED ${PLUGIN_SOURCES})

target_link_libraries(plugin-script PRIVATE plugin-shared qjs)

target_include_directories(plugin-script PRIVATE
    ${quickjs_ng_SOURCE_DIR}
)

# QuickJS requires a larger stack than the MSVC default
target_link_options(plugin-script PRIVATE /STACK:8388608)

set_target_properties(plugin-script PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
