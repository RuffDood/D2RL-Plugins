#pragma once

#include <json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ruffneckk::plugin_items::ethereal {

constexpr std::size_t MaxExcludedItemTypes = 64;
constexpr std::size_t ItemTypeRecordStride = 0xE8;
constexpr std::uint8_t VanillaChancePercent = 5;

struct ItemTypeCode {
	std::array<char, 4> bytes{' ', ' ', ' ', ' '};
	std::array<char, 5> text{};
	std::uint8_t length{};
};

struct EtherealExclusions {
	bool enabled{};
	std::array<ItemTypeCode, MaxExcludedItemTypes> itemTypes{};
	std::size_t itemTypeCount{};
};

struct EtherealItemRules {
	bool enabled{};
	std::uint8_t chancePercent{VanillaChancePercent};
	bool allowSetItems{};
	bool allowIndestructibleItems{};
};

struct Config {
	EtherealExclusions exclusions{};
	EtherealItemRules rules{};
};

inline bool NormalizeItemTypeCode(std::string_view input, ItemTypeCode& output) noexcept {
	while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) {
		input.remove_prefix(1);
	}
	while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) {
		input.remove_suffix(1);
	}
	if (input.empty() || input.size() > 4) return false;

	output = {};
	output.bytes.fill(' ');
	output.length = static_cast<std::uint8_t>(input.size());
	for (std::size_t index = 0; index < input.size(); ++index) {
		const auto character = static_cast<unsigned char>(input[index]);
		if (!std::isalnum(character) && character != '_') return false;
		const auto normalized = static_cast<char>(std::tolower(character));
		output.bytes[index] = normalized;
		output.text[index] = normalized;
	}
	return true;
}

inline bool SameCode(const ItemTypeCode& left, const ItemTypeCode& right) noexcept {
	return left.bytes == right.bytes;
}

inline std::int32_t FindItemTypeId(
	const void* records,
	std::uint64_t count,
	std::size_t stride,
	const ItemTypeCode& code
) noexcept {
	if (!records || stride < code.bytes.size() || count > 4096) return -1;
	const auto* bytes = static_cast<const std::uint8_t*>(records);
	for (std::uint64_t index = 0; index < count; ++index) {
		if (std::memcmp(bytes + index * stride, code.bytes.data(), code.bytes.size()) == 0) {
			return static_cast<std::int32_t>(index);
		}
	}
	return -1;
}

inline void RequireAllowedKeys(
	const nlohmann::json& object,
	std::initializer_list<std::string_view> allowed,
	std::string_view context
) {
	if (!object.is_object()) {
		throw std::runtime_error(std::string(context) + " must be an object");
	}
	for (const auto& [key, value] : object.items()) {
		(void)value;
		if (std::find(allowed.begin(), allowed.end(), std::string_view(key)) == allowed.end()) {
			throw std::runtime_error(
				std::string(context) + " contains unknown key '" + key + "'"
			);
		}
	}
}

// The input is the complete `items` object. Other PluginPack item options are
// deliberately ignored; only these two RuffnecKk-owned sections are parsed.
inline Config ParseConfig(const nlohmann::json& items) {
	if (!items.is_object()) throw std::runtime_error("items must be an object");

	Config parsed{};
	if (items.contains("etherealExclusions")) {
		const auto& exclusions = items.at("etherealExclusions");
		RequireAllowedKeys(exclusions, {"enabled", "itemTypes"}, "items.etherealExclusions");

		if (exclusions.contains("enabled")) {
			if (!exclusions.at("enabled").is_boolean()) {
				throw std::runtime_error("items.etherealExclusions.enabled must be true or false");
			}
			parsed.exclusions.enabled = exclusions.at("enabled").get<bool>();
		}
		if (exclusions.contains("itemTypes")) {
			const auto& itemTypes = exclusions.at("itemTypes");
			if (!itemTypes.is_array()) {
				throw std::runtime_error("items.etherealExclusions.itemTypes must be an array");
			}
			for (std::size_t index = 0; index < itemTypes.size(); ++index) {
				const auto& value = itemTypes.at(index);
				if (!value.is_string()) {
					throw std::runtime_error(
						"items.etherealExclusions.itemTypes[" + std::to_string(index)
						+ "] must be a string"
					);
				}
				ItemTypeCode code{};
				if (!NormalizeItemTypeCode(value.get_ref<const std::string&>(), code)) {
					throw std::runtime_error(
						"items.etherealExclusions.itemTypes[" + std::to_string(index)
						+ "] must be a 1-4 character itemtypes code"
					);
				}
				bool duplicate{};
				for (std::size_t existing = 0;
					existing < parsed.exclusions.itemTypeCount;
					++existing) {
					if (SameCode(parsed.exclusions.itemTypes[existing], code)) {
						duplicate = true;
						break;
					}
				}
				if (duplicate) continue;
				if (parsed.exclusions.itemTypeCount >= MaxExcludedItemTypes) {
					throw std::runtime_error(
						"items.etherealExclusions.itemTypes supports at most 64 unique codes"
					);
				}
				parsed.exclusions.itemTypes[parsed.exclusions.itemTypeCount++] = code;
			}
		}
	}

	if (items.contains("etherealItemRules")) {
		const auto& rules = items.at("etherealItemRules");
		RequireAllowedKeys(
			rules,
			{"enabled", "chancePercent", "allowSetItems", "allowIndestructibleItems"},
			"items.etherealItemRules"
		);

		if (rules.contains("enabled")) {
			if (!rules.at("enabled").is_boolean()) {
				throw std::runtime_error("items.etherealItemRules.enabled must be true or false");
			}
			parsed.rules.enabled = rules.at("enabled").get<bool>();
		}
		if (rules.contains("chancePercent")) {
			if (!rules.at("chancePercent").is_number_integer()) {
				throw std::runtime_error("items.etherealItemRules.chancePercent must be an integer");
			}
			const auto chance = rules.at("chancePercent").get<std::int64_t>();
			if (chance < 0 || chance > 100) {
				throw std::runtime_error(
					"items.etherealItemRules.chancePercent must be from 0 through 100"
				);
			}
			parsed.rules.chancePercent = static_cast<std::uint8_t>(chance);
		}
		if (rules.contains("allowSetItems")) {
			if (!rules.at("allowSetItems").is_boolean()) {
				throw std::runtime_error("items.etherealItemRules.allowSetItems must be true or false");
			}
			parsed.rules.allowSetItems = rules.at("allowSetItems").get<bool>();
		}
		if (rules.contains("allowIndestructibleItems")) {
			if (!rules.at("allowIndestructibleItems").is_boolean()) {
				throw std::runtime_error(
					"items.etherealItemRules.allowIndestructibleItems must be true or false"
				);
			}
			parsed.rules.allowIndestructibleItems =
				rules.at("allowIndestructibleItems").get<bool>();
		}
	}
	return parsed;
}

inline bool PatchChance(const Config& config) noexcept {
	return config.rules.enabled && config.rules.chancePercent != VanillaChancePercent;
}

inline bool PatchSetItems(const Config& config) noexcept {
	return config.rules.enabled && config.rules.allowSetItems;
}

inline bool PatchIndestructibleItems(const Config& config) noexcept {
	return config.rules.enabled && config.rules.allowIndestructibleItems;
}

inline bool HasDirectRulePatches(const Config& config) noexcept {
	return PatchChance(config) || PatchSetItems(config) || PatchIndestructibleItems(config);
}

} // namespace ruffneckk::plugin_items::ethereal
