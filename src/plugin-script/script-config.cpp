#include "script-config.h"
#include <Windows.h>
#include <string>

void ScriptPluginOptions::Load(const D2RLoaderPluginContext* /*context*/, const nlohmann::json& cfg) {
    bEnabled        = cfg.value("enabled",        true);
    scriptDirectory = cfg.value("scriptDirectory", std::string{});
    memoryLimitMB   = cfg.value("memoryLimitMB",  64u);
    stackSizeKB     = cfg.value("stackSizeKB",    512u);
}

std::string ScriptPluginOptions::ResolveScriptDir(const D2RLoaderPluginContext* context) const {
    if (!scriptDirectory.empty()) return scriptDirectory;

    // Default: {modDirectory}/scripts
    if (context && context->modDirectory) {
        int len = WideCharToMultiByte(CP_UTF8, 0, context->modDirectory, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) {
            std::string modPath(static_cast<size_t>(len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, context->modDirectory, -1, modPath.data(), len, nullptr, nullptr);
            return modPath + "/scripts";
        }
    }
    return "scripts";
}
