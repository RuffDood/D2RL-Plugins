#pragma once

#include <plugin-shared-json.h>
#include <plugin.h>
#include <string>

struct ScriptPluginOptions {
    bool        bEnabled        = true;
    std::string scriptDirectory;   // empty = default: modDirectory/scripts
    uint32_t    memoryLimitMB   = 64;
    uint32_t    stackSizeKB     = 512;

    void Load(const D2RLoaderPluginContext* context, const nlohmann::json& cfg);

    // Resolved at load time from context->modDirectory + scriptDirectory override.
    // Returns the directory to scan for .js files (UTF-8).
    std::string ResolveScriptDir(const D2RLoaderPluginContext* context) const;
};
