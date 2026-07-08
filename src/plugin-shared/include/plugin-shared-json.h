#pragma once
#include <json.hpp>
#include <D2RLPlugin/context.h>
#include <fstream>
#include <optional>
#include <string>
#include <Windows.h>

// Tries <modDirectory>/D2RPlugins.json, then ./D2RPlugins.json.
// Returns the parsed JSON on success, or nullopt if neither file is found or is malformed.
inline std::optional<nlohmann::json> PSh_Json_LoadConfig(const D2RL::PluginContext* context)
{
    auto tryLoad = [](const std::string& path) -> std::optional<nlohmann::json> {
        std::ifstream f(path);
        if (!f.is_open()) return std::nullopt;
        try { return nlohmann::json::parse(f, nullptr, true, true); }
        catch (...) { return std::nullopt; }
    };

    if (context && context->modDirectory) {
        int len = WideCharToMultiByte(CP_UTF8, 0, context->modDirectory, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) {
            std::string modPath(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, context->modDirectory, -1, modPath.data(), len, nullptr, nullptr);
            modPath += "/D2RPlugins.json";
            if (auto j = tryLoad(modPath)) return j;
        }
    }
    return tryLoad("D2RPlugins.json");
}

// Returns the named top-level section from a loaded config, or an empty object if missing.
inline nlohmann::json PSh_Json_GetSection(const std::optional<nlohmann::json>& cfg, const char* section)
{
    if (!cfg) return nlohmann::json::object();
    return cfg->value(section, nlohmann::json::object());
}
