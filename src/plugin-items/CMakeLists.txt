include(FetchContent)

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	# v1.91.5
	GIT_TAG f401021d5a5d56fe2304056c391e78f81c8d4b8f
)
FetchContent_Declare(
	minhook
	GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
	# v1.3.4
	GIT_TAG c3fcafdc10146beb5919319d0683e44e3c30d537
)
FetchContent_MakeAvailable(imgui minhook)

add_library(plugin-items-extended-imgui STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(plugin-items-extended-imgui PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)
target_compile_features(plugin-items-extended-imgui PUBLIC cxx_std_20)
target_compile_definitions(
	plugin-items-extended-imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(plugin-items-extended-imgui PUBLIC dxgi)

add_library(plugin-items-extended-transport STATIC
	extended-item-stats-transport.cpp
)
target_compile_features(plugin-items-extended-transport PUBLIC cxx_std_20)
target_include_directories(
	plugin-items-extended-transport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(plugin-items-extended-tooltip STATIC
	extended-item-stats-tooltip.cpp
)
target_compile_features(plugin-items-extended-tooltip PUBLIC cxx_std_20)
target_include_directories(
	plugin-items-extended-tooltip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM PLUGIN_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/extended-item-stats-transport.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extended-item-stats-tooltip.cpp
)

add_library(plugin-items SHARED ${PLUGIN_SOURCES} plugin.rc)

target_link_libraries(plugin-items PRIVATE
	plugin-shared
	plugin-items-extended-transport
	plugin-items-extended-tooltip
	plugin-items-extended-imgui
	minhook
	Xinput
	dxgi
)

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

	add_executable(item-durability-policy-tests
		tests/item-durability-policy-tests.cpp
	)
	target_link_libraries(item-durability-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		item-durability-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME item-durability-policy
		COMMAND item-durability-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(charm-aura-trigger-fix-policy-tests
		tests/charm-aura-trigger-fix-policy-tests.cpp
	)
	target_link_libraries(charm-aura-trigger-fix-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		charm-aura-trigger-fix-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME charm-aura-trigger-fix-policy
		COMMAND charm-aura-trigger-fix-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(extended-item-stats-transport-tests
		tests/extended-item-stats-transport-tests.cpp
	)
	target_link_libraries(
		extended-item-stats-transport-tests PRIVATE plugin-items-extended-transport)
	add_test(
		NAME extended-item-stats-transport
		COMMAND extended-item-stats-transport-tests
	)

	add_executable(extended-item-stats-policy-tests
		tests/extended-item-stats-policy-tests.cpp
	)
	target_link_libraries(extended-item-stats-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		extended-item-stats-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME extended-item-stats-policy
		COMMAND extended-item-stats-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(extended-item-stats-tooltip-tests
		tests/extended-item-stats-tooltip-tests.cpp
	)
	target_link_libraries(
		extended-item-stats-tooltip-tests PRIVATE plugin-items-extended-tooltip)
	add_test(
		NAME extended-item-stats-tooltip
		COMMAND extended-item-stats-tooltip-tests
	)

	add_executable(qty-display-issue-policy-tests
		tests/qty-display-issue-policy-tests.cpp
	)
	target_link_libraries(qty-display-issue-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		qty-display-issue-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME qty-display-issue-policy
		COMMAND qty-display-issue-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(vendor-stock-refresh-policy-tests
		tests/vendor-stock-refresh-policy-tests.cpp
	)
	target_link_libraries(vendor-stock-refresh-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		vendor-stock-refresh-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME vendor-stock-refresh-policy
		COMMAND vendor-stock-refresh-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)

	add_executable(vendor-overhaul-policy-tests
		tests/vendor-overhaul-policy-tests.cpp
	)
	target_link_libraries(vendor-overhaul-policy-tests PRIVATE plugin-shared)
	target_include_directories(vendor-overhaul-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME vendor-overhaul-policy
		COMMAND vendor-overhaul-policy-tests
	)

	add_executable(magic-find-formula-policy-tests
		tests/magic-find-formula-policy-tests.cpp
	)
	target_link_libraries(magic-find-formula-policy-tests PRIVATE plugin-shared)
	target_include_directories(
		magic-find-formula-policy-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(
		NAME magic-find-formula-policy
		COMMAND magic-find-formula-policy-tests ${CMAKE_SOURCE_DIR}/D2RPlugins.json
	)
endif()
