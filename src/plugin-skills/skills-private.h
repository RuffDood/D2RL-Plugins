#pragma once

#include <plugin-shared.h>
#include <plugin-shared-json.h>

struct SkillPluginOptions {
	bool bEnableManaCostsLife;          // When enabled, the "ManaCostsLife" field will be read from skills.txt
	bool bEnableManaCostsStamina;       // When enabled, the "ManaCostsStamina" field will be read from skills.txt
	bool bEnableClassicWW;              // When enabled, Whirlwind has no cooldown between individual hits
	bool bEnableWWCtc;                  // When enabled, Whirlwind will issue CtC events
	bool bTelekinesisPicksUpEverything; // When enabled, Telekinesis picks up anything
	bool bEnableChargedPctDrainStat;    // When enabled, charged items have a chance to not drain a charge on use
	int  ChargedPctDrainStat;           // Stat ID giving the % chance to skip the charge drain

	void Load(const D2RLoaderPluginContext* context, const nlohmann::json& cfg);
};