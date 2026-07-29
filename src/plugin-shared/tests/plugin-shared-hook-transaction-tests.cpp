#include <plugin-shared.h>

#include <cassert>
#include <vector>

int main()
{
	std::vector<int> calls;

	assert(PSh_HookTransactionBegin());
	assert(PSh_HookTransactionEnqueue("first", true, [&] {
		calls.push_back(1);
		return true;
	}));
	assert(PSh_HookTransactionEnqueue("second", true, [&] {
		calls.push_back(2);
		return true;
	}));
	assert(calls.empty());
	PSh_HookTransactionAbort();
	assert(calls.empty());

	assert(PSh_HookTransactionBegin());
	assert(PSh_HookTransactionEnqueue("first", true, [&] {
		calls.push_back(1);
		return true;
	}));
	assert(PSh_HookTransactionEnqueue("second", true, [&] {
		calls.push_back(2);
		return true;
	}));
	const auto success = PSh_HookTransactionCommit(nullptr);
	assert(success.success);
	assert(success.completedOperations == 2);
	assert(success.totalOperations == 2);
	assert((calls == std::vector<int>{ 1, 2 }));

	calls.clear();
	assert(PSh_HookTransactionBegin());
	assert(PSh_HookTransactionEnqueue("optional", false, [&] {
		calls.push_back(1);
		return false;
	}));
	assert(PSh_HookTransactionEnqueue("required", true, [&] {
		calls.push_back(2);
		return true;
	}));
	const auto optionalFailure = PSh_HookTransactionCommit(nullptr);
	assert(optionalFailure.success);
	assert(optionalFailure.completedOperations == 1);
	assert(optionalFailure.totalOperations == 2);
	assert((calls == std::vector<int>{ 1, 2 }));

	calls.clear();
	assert(PSh_HookTransactionBegin());
	assert(PSh_HookTransactionEnqueue("installed", true, [&] {
		calls.push_back(1);
		return true;
	}));
	assert(PSh_HookTransactionEnqueue("refused", true, [&] {
		calls.push_back(2);
		return false;
	}));
	assert(PSh_HookTransactionEnqueue("must-not-run", true, [&] {
		calls.push_back(3);
		return true;
	}));
	const auto requiredFailure = PSh_HookTransactionCommit(nullptr);
	assert(!requiredFailure.success);
	assert(requiredFailure.completedOperations == 1);
	assert(requiredFailure.totalOperations == 3);
	assert((calls == std::vector<int>{ 1, 2 }));

	assert(PSh_HookTransactionBegin());
	assert(!PSh_HookTransactionBegin());
	PSh_HookTransactionAbort();

	return 0;
}
