#pragma once

#include <plugin-shared.h>

struct LevelPluginOptions {
	bool bDisableAct1Path;

	void Load(const D2RLoaderPluginContext* context, const wchar_t* section);
};
