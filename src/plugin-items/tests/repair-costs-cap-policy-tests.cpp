#include "repair-costs-cap-policy.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
	using namespace RuffnecKk::RepairCostsCap;

	assert(IsValidMaximumGold(0));
	assert(IsValidMaximumGold(MaximumGoldLimit));
	assert(!IsValidMaximumGold(-1));
	assert(!IsValidMaximumGold(MaximumGoldLimit + 1));
	assert(IsValidChance(0.0));
	assert(IsValidChance(0.10));
	assert(IsValidChance(1.0));
	assert(!IsValidChance(-0.01));
	assert(!IsValidChance(1.01));
	assert(ChanceToBasisPoints(0.0) == 0);
	assert(ChanceToBasisPoints(0.10) == 1'000);
	assert(ChanceToBasisPoints(1.0) == ChanceBasisPointScale);

	const RepairPolicy vanilla{
		.enabled = true,
		.maximumGold = std::numeric_limits<std::int32_t>::max(),
		.durabilityWearEnabled = true,
		.durabilityWearChance = 0.0,
	};
	assert(ApplyRepairCostCap(50'000, RepairTransactionType, vanilla) == 50'000);
	assert(ApplyRepairAllCap(30'000, vanilla) == 30'000);
	assert(!ShouldLoseMaximumDurability(true, vanilla.durabilityWearChance, 0));

	RepairPolicy policy{
		.enabled = true,
		.maximumGold = 5'000,
		.durabilityWearEnabled = true,
		.durabilityWearChance = 0.10,
	};
	assert(IsValidPolicy(policy));

	assert(ApplyRepairCostCap(2'500, RepairTransactionType, policy) == 2'500);
	assert(ApplyRepairCostCap(50'000, RepairTransactionType, policy) == 5'000);
	assert(ApplyRepairCostCap(10'000, 0, policy) == 10'000);
	assert(ApplyRepairCostCap(0, RepairTransactionType, policy) == 0);
	assert(ApplyRepairCostCap(-1, RepairTransactionType, policy) == -1);
	assert(ApplyRepairCostCap(
		std::numeric_limits<std::int32_t>::max(),
		RepairTransactionType,
		policy
	) == std::numeric_limits<std::int32_t>::max());

	assert(ApplyRepairAllCap(2'500, policy) == 2'500);
	assert(ApplyRepairAllCap(30'000, policy) == 5'000);
	assert(ApplyRepairAllCap(0, policy) == 0);
	assert(ApplyRepairAllCap(
		std::numeric_limits<std::int32_t>::max(),
		policy
	) == std::numeric_limits<std::int32_t>::max());

	policy.enabled = false;
	assert(ApplyRepairCostCap(50'000, RepairTransactionType, policy) == 50'000);
	assert(ApplyRepairAllCap(30'000, policy) == 30'000);
	policy.enabled = true;

	auto free = policy;
	free.maximumGold = 0;
	assert(ApplyRepairCostCap(50'000, RepairTransactionType, free) == 0);
	assert(ApplyRepairCostCap(50'000, 0, free) == 50'000);
	assert(ApplyRepairCostCap(0, RepairTransactionType, free) == 0);
	assert(ApplyRepairCostCap(
		std::numeric_limits<std::int32_t>::max(),
		RepairTransactionType,
		free
	) == std::numeric_limits<std::int32_t>::max());
	assert(ApplyRepairAllCap(30'000, free) == 0);
	assert(ApplyRepairAllCap(0, free) == 0);

	auto invalid = policy;
	invalid.maximumGold = -1;
	assert(!IsValidPolicy(invalid));
	assert(ApplyRepairCostCap(50'000, RepairTransactionType, invalid) == 50'000);
	assert(ApplyRepairAllCap(30'000, invalid) == 30'000);

	assert(GoldReduction(50'000, 5'000) == 45'000);
	assert(GoldReduction(5'000, 5'000) == 0);
	assert(GoldReduction(5'000, 10'000) == 0);
	assert(GoldReduction(-1, 0) == 0);

	assert(IsPhysicalRepairCandidate(0, 20));
	assert(IsPhysicalRepairCandidate(19, 20));
	assert(!IsPhysicalRepairCandidate(20, 20));
	assert(!IsPhysicalRepairCandidate(0, 1));
	assert(!IsPhysicalRepairCandidate(-1, 20));
	assert(DidPhysicalRepairSucceed(10, 20, 20));
	assert(!DidPhysicalRepairSucceed(10, 20, 19));
	assert(!DidPhysicalRepairSucceed(20, 20, 20));

	assert(!ShouldLoseMaximumDurability(false, 1.0, 0));
	assert(!ShouldLoseMaximumDurability(true, 0.0, 0));
	assert(ShouldLoseMaximumDurability(true, 0.10, 0));
	assert(ShouldLoseMaximumDurability(true, 0.10, 999));
	assert(!ShouldLoseMaximumDurability(true, 0.10, 1'000));
	assert(ShouldLoseMaximumDurability(true, 1.0, 9'999));
	assert(!ShouldLoseMaximumDurability(true, 1.0, 10'000));
	assert(ReducedMaximumDurability(20) == 19);
	assert(ReducedMaximumDurability(2) == 1);
	assert(ReducedMaximumDurability(1) == 1);
	return 0;
}
