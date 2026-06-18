#pragma once

#include <plugin-shared.h>
#include <plugin-shared-json.h>

struct LevelPluginOptions {
	bool bDisableAct1Path;

	void Load(const D2RLoaderPluginContext* context, const nlohmann::json& cfg);
};
