#include <plugin-shared-json.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

void WriteFile(const std::filesystem::path& path, std::string_view contents)
{
	std::ofstream file(path, std::ios::binary);
	assert(file.is_open());
	file << contents;
	assert(file.good());
}

template <typename Callback>
bool Throws(Callback&& callback)
{
	try {
		callback();
		return false;
	}
	catch (const std::exception&) {
		return true;
	}
}

} // namespace

int main()
{
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto root = std::filesystem::temp_directory_path()
		/ ("pluginpack-json-tests-" + std::to_string(unique));
	const auto modDirectory = root / "mod";
	const auto globalDirectory = root / "global";
	std::filesystem::create_directories(modDirectory);
	std::filesystem::create_directories(globalDirectory);

	const auto modConfig = modDirectory / "D2RPlugins.json";
	const auto globalConfig = globalDirectory / "D2RPlugins.json";

	const auto absent = PSh_Json_Detail::LoadConfigFromPaths(modConfig, globalConfig);
	assert(!absent);

	WriteFile(globalConfig, R"json({
		// JSON comments remain supported.
		"items": { "extendedItemStats": { "enabled": true } }
	})json");
	const auto global = PSh_Json_Detail::LoadConfigFromPaths(modConfig, globalConfig);
	assert(global);
	assert(PSh_Json_GetSection(global, "items").at("extendedItemStats").at("enabled") == true);

	WriteFile(modConfig, R"json({ "items": { "extendedItemStats": { "enabled": false } } })json");
	const auto local = PSh_Json_Detail::LoadConfigFromPaths(modConfig, globalConfig);
	assert(local);
	assert(PSh_Json_GetSection(local, "items").at("extendedItemStats").at("enabled") == false);

	WriteFile(modConfig, "{ invalid json");
	assert(Throws([&] { (void)PSh_Json_Detail::LoadConfigFromPaths(modConfig, globalConfig); }));

	std::filesystem::remove(modConfig);
	WriteFile(globalConfig, "[]");
	assert(Throws([&] { (void)PSh_Json_Detail::LoadConfigFromPaths(modConfig, globalConfig); }));

	WriteFile(globalConfig, R"json({ "items": false })json");
	const auto wrongSection = PSh_Json_Detail::LoadConfigFromPaths(modConfig, globalConfig);
	assert(Throws([&] { (void)PSh_Json_GetSection(wrongSection, "items"); }));

	std::filesystem::remove_all(root);
	return 0;
}
