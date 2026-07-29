file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS *.cpp)

add_library(plugin-items SHARED ${PLUGIN_SOURCES} plugin.rc)

target_link_libraries(plugin-items PRIVATE plugin-shared)

set_target_properties(plugin-items PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(BUILD_TESTING)
	add_executable(items-ethereal-policy-tests
		tests/items-ethereal-policy-tests.cpp
	)
	target_link_libraries(items-ethereal-policy-tests PRIVATE plugin-shared)
	target_include_directories(items-ethereal-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME items-ethereal-policy
		COMMAND items-ethereal-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(repair-costs-cap-policy-tests
		tests/repair-costs-cap-policy-tests.cpp
	)
	target_link_libraries(repair-costs-cap-policy-tests PRIVATE plugin-shared)
	target_include_directories(repair-costs-cap-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME repair-costs-cap-policy
		COMMAND repair-costs-cap-policy-tests
	)

	add_executable(gamble-screen-limit-policy-tests
		tests/gamble-screen-limit-policy-tests.cpp
	)
	target_link_libraries(gamble-screen-limit-policy-tests PRIVATE plugin-shared)
	target_include_directories(gamble-screen-limit-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME gamble-screen-limit-policy
		COMMAND gamble-screen-limit-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(ground-item-label-limit-policy-tests
		tests/ground-item-label-limit-policy-tests.cpp
	)
	target_link_libraries(ground-item-label-limit-policy-tests PRIVATE plugin-shared)
	target_include_directories(ground-item-label-limit-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME ground-item-label-limit-policy
		COMMAND ground-item-label-limit-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(enhanced-damage-min-max-fix-policy-tests
		tests/enhanced-damage-min-max-fix-policy-tests.cpp
	)
	target_link_libraries(enhanced-damage-min-max-fix-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		enhanced-damage-min-max-fix-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME enhanced-damage-min-max-fix-policy
		COMMAND enhanced-damage-min-max-fix-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)
endif()
