#pragma once

#include <json.hpp>

#include <stdexcept>
#include <string>

namespace RuffnecKk::ExtendedItemStats {

struct Config {
	bool enabled{true};
};

inline Config ParseConfig(const nlohmann::json& itemsConfig) {
	if (!itemsConfig.is_object()) {
		throw std::invalid_argument("items must be an object");
	}

	const auto entry = itemsConfig.find("extendedItemStats");
	if (entry == itemsConfig.end()) return {};
	if (!entry->is_object()) {
		throw std::invalid_argument("items.extendedItemStats must be an object");
	}
	for (const auto& [key, value] : entry->items()) {
		(void)value;
		if (key != "enabled") {
			throw std::invalid_argument(
				"items.extendedItemStats has unknown setting: " + key);
		}
	}
	if (!entry->contains("enabled") || !entry->at("enabled").is_boolean()) {
		throw std::invalid_argument(
			"items.extendedItemStats.enabled must be a boolean");
	}
	return {.enabled = entry->at("enabled").get<bool>()};
}

} // namespace RuffnecKk::ExtendedItemStats
