#include <D2RLPlugin/api.h>
#include "levels-private.h"

// ── Addresses (offsets from exe base 0x140000000) ────────────────────────────

static constexpr uint64_t OFF_DisableAct1DirtPath1 = 0x2D877C;
static constexpr uint64_t OFF_DisableAct1DirtPath2 = 0x2D8789;

// ── Plugin state ──────────────────────────────────────────────────────────────

static LevelPluginOptions g_pluginOptions;

// ── JSON loading ──────────────────────────────────────────────────────────────

void LevelPluginOptions::Load(const D2RL::PluginContext* /*context*/, const nlohmann::json& cfg)
{
	bDisableAct1Path = cfg.value("disableAct1Path", false);
}

// ── Plugin exports ────────────────────────────────────────────────────────────

static constexpr D2RL::PluginInfo PluginInfo {
	.infoSize   = D2RL::PluginInfoSize,
	.apiVersion = D2RL_PLUGIN_API_VERSION,
	.id         = "plugin-levels",
	.name       = "Levels Plugin",
	.version    = "0.0.1",
	.author     = "eezstreet",
	.description = "Various level-related changes.",
	.flags      = D2RL::PluginFlags::None,
};

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &PluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}

	auto cfg = PSh_Json_LoadConfig(context);
	g_pluginOptions.Load(context, PSh_Json_GetSection(cfg, "levels"));

	if (g_pluginOptions.bDisableAct1Path)
	{
		unsigned char patch1[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
		(void)context->PatchBytes(OFF_DisableAct1DirtPath1, nullptr, 0, patch1, sizeof(patch1));

		unsigned char patch2[] = { 0xE9, 0x98, 0x00, 0x00, 0x00, 0x90 };
		(void)context->PatchBytes(OFF_DisableAct1DirtPath2, nullptr, 0, patch2, sizeof(patch2));
	}

	return true;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderUnloadPlugin() noexcept {
	// Byte patches installed via context->PatchBytes are reverted automatically by
	// D2RLoader on unload (ASSUMPTION — verify against real loader behavior before
	// relying on this in production).
}
