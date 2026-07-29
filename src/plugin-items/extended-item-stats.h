#pragma once

#include <D2RLPlugin/context.h>
#include <json.hpp>

namespace RuffnecKk::ExtendedItemStats {

bool Load(
	const D2RL::PluginContext* context,
	const nlohmann::json& itemsConfig) noexcept;
void Unload() noexcept;

} // namespace RuffnecKk::ExtendedItemStats
