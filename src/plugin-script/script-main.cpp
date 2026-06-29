#include "plugin.h"
#include "script-private.h"
#include "script-config.h"
#include "script-dispatch.h"
#include "script-runtime.h"

// ── Plugin metadata ───────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
    .apiVersion = D2RLOADER_PLUGIN_API_VERSION,
    .id         = "plugin-script",
    .name       = "Script Plugin",
    .version    = "0.0.1",
    .author     = "eezstreet",
    .flags      = D2RLoaderPluginFlag_None,
};

// ── Plugin exports ────────────────────────────────────────────────────────────

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
    return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
    if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION) return false;

    g_ExeBase = context->exeBase;
    g_Context = context;

    auto cfg = PSh_Json_LoadConfig(context);
    ScriptPluginOptions opts{};
    opts.Load(context, PSh_Json_GetSection(cfg, "script"));

    if (!opts.bEnabled) return true;

    if (!Script_RuntimeInit(opts)) {
        D2RPluginLogErrorF(context, "plugin-script: failed to initialize QuickJS runtime");
        return false;
    }

    std::string scriptDir = opts.ResolveScriptDir(context);
    Script_LoadScripts(scriptDir, context);
    Script_InstallDispatchHooks(context);

    return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
    Script_RemoveDispatchHooks();
    Script_ClearRegistry();
    Script_RuntimeShutdown();
    g_Context = nullptr;
    g_ExeBase = 0;
}
