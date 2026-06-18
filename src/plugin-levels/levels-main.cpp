#include "plugin.h"
#include "levels-private.h"

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_DisableAct1DirtPath1 = 0x2D877C;
static constexpr uint64_t OFF_DisableAct1DirtPath2 = 0x2D8789;

// ── Plugin state ──────────────────────────────────────────────────────────────

static LevelPluginOptions g_pluginOptions;

// ── JSON loading ──────────────────────────────────────────────────────────────

void LevelPluginOptions::Load(const D2RLoaderPluginContext* /*context*/, const nlohmann::json& cfg)
{
	bDisableAct1Path = cfg.value("disableAct1Path", false);
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RLoaderPluginInfo PluginInfo {
	.apiVersion = D2RLOADER_PLUGIN_API_VERSION,
	.id         = "plugin-levels",
	.name       = "Levels Plugin",
	.version    = "1.0.0",
	.author     = "eezstreet",
	.flags      = D2RLoaderPluginFlag_None,
};

D2RLOADER_PLUGIN_EXPORT const D2RLoaderPluginInfo* __cdecl D2RLoaderGetPluginInfo() noexcept {
	return &PluginInfo;
}

D2RLOADER_PLUGIN_EXPORT bool __cdecl D2RLoaderLoadHooks(const D2RLoaderPluginContext* context) noexcept {
	if (!context || context->apiVersion < D2RLOADER_PLUGIN_API_VERSION) {
		return false;
	}

	auto cfg = PSh_Json_LoadConfig(context);
	g_pluginOptions.Load(context, PSh_Json_GetSection(cfg, "levels"));

	if (g_pluginOptions.bDisableAct1Path)
	{
		unsigned char patch1[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
		PSh_PatchBytes(PLUGINID_LEVELS, context, OFF_DisableAct1DirtPath1, 5, patch1);

		unsigned char patch2[] = { 0xE9, 0x98, 0x00, 0x00, 0x00, 0x90 };
		PSh_PatchBytes(PLUGINID_LEVELS, context, OFF_DisableAct1DirtPath2, 6, patch2);
	}

	return true;
}

D2RLOADER_PLUGIN_EXPORT void __cdecl D2RLoaderUnload() noexcept {
}
