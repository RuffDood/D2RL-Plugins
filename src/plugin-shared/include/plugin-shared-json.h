#pragma once

#include <D2RLPlugin/context.h>
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <optional>
#include <stdexcept>
#include <string>

namespace PSh_Json_Detail {

struct FileAttempt {
	bool found{};
	std::optional<nlohmann::json> value;
};

inline FileAttempt TryLoadFile(const std::filesystem::path& path)
{
	std::error_code fileError;
	const bool exists = std::filesystem::exists(path, fileError);
	if (fileError) {
		throw std::runtime_error(
			"PluginPack: could not inspect configuration file '" + path.string()
			+ "': " + fileError.message());
	}
	if (!exists) {
		return {};
	}

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error(
			"PluginPack: configuration file '" + path.string() + "' could not be opened.");
	}

	nlohmann::json parsed;
	try {
		parsed = nlohmann::json::parse(file, nullptr, true, true);
	}
	catch (const std::exception& error) {
		throw std::runtime_error(
			"PluginPack: configuration file '" + path.string() + "' is invalid: " + error.what());
	}

	if (!parsed.is_object()) {
		throw std::runtime_error(
			"PluginPack: configuration file '" + path.string() + "' must contain a JSON object.");
	}

	return { true, std::move(parsed) };
}

inline std::optional<nlohmann::json> LoadConfigFromPaths(
	const std::optional<std::filesystem::path>& modConfig,
	const std::filesystem::path& globalConfig)
{
	if (modConfig) {
		auto attempt = TryLoadFile(*modConfig);
		if (attempt.found) {
			return std::move(attempt.value);
		}
	}

	auto attempt = TryLoadFile(globalConfig);
	return attempt.found ? std::move(attempt.value) : std::nullopt;
}

} // namespace PSh_Json_Detail

// Tries <modDirectory>/D2RPlugins.json, then ./D2RPlugins.json.
// A missing file is allowed. A present but unreadable or invalid file is rejected,
// and an invalid mod-local file never silently falls back to the global file.
inline std::optional<nlohmann::json> PSh_Json_LoadConfig(const D2RL::PluginContext* context)
{
	std::optional<std::filesystem::path> modConfig;
	if (context && context->modDirectory && context->modDirectory[0] != L'\0') {
		modConfig = std::filesystem::path(context->modDirectory) / L"D2RPlugins.json";
	}
	return PSh_Json_Detail::LoadConfigFromPaths(
		modConfig,
		std::filesystem::path(L"D2RPlugins.json"));
}

// Returns the named top-level section from a loaded config, or an empty object if missing.
// A present section of the wrong JSON type is rejected instead of being treated as absent.
inline nlohmann::json PSh_Json_GetSection(
	const std::optional<nlohmann::json>& cfg,
	const char* section)
{
	if (!cfg) {
		return nlohmann::json::object();
	}

	const auto entry = cfg->find(section);
	if (entry == cfg->end()) {
		return nlohmann::json::object();
	}
	if (!entry->is_object()) {
		throw std::runtime_error(
			std::string("PluginPack: configuration section '") + section + "' must be a JSON object.");
	}
	return *entry;
}

inline void PSh_Json_LogConfigError(
	const D2RL::PluginContext* context,
	const std::exception& error) noexcept
{
	if (context) {
		context->LogError(error.what());
	}
}
