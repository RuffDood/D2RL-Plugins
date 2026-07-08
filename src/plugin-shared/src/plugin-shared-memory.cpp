#include <D2RLPlugin/api.h>

/**
 *	This just exists to shut D2RLoader up.
 */
static constexpr D2RL::PluginInfo PluginInfo{
	.infoSize = D2RL::PluginInfoSize,
	.apiVersion = D2RL_PLUGIN_API_VERSION,
	.id = "plugin-shared",
	.name = "Plugin Shared Data",
	.version = "1.0.0",
	.author = "eezstreet",
	.description = "Shared data/utilities for eezstreet plugins.",
	.flags = D2RL::PluginFlags::None,
};

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &PluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	return context != nullptr;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderUnloadPlugin() noexcept {
}
