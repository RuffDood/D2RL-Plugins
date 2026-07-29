#include "items-private.h"

#include <cassert>
#include <cstdint>

int main()
{
	assert(PSh_Items_IsValidVendorLevelScale(0));
	assert(PSh_Items_IsValidVendorLevelScale(1));
	assert(PSh_Items_IsValidVendorLevelScale(16));
	assert(PSh_Items_IsValidVendorLevelScale(128));
	assert(!PSh_Items_IsValidVendorLevelScale(-1));
	assert(!PSh_Items_IsValidVendorLevelScale(3));
	assert(!PSh_Items_IsValidVendorLevelScale(127));

	assert(!PSh_Items_ShouldGenerateRareVendorItem(false, 0, 1024));
	assert(!PSh_Items_ShouldGenerateRareVendorItem(true, 0, 0));
	assert(PSh_Items_ShouldGenerateRareVendorItem(true, 0, 1024));
	assert(!PSh_Items_ShouldGenerateRareVendorItem(true, 1, 1024));
	assert(!PSh_Items_ShouldGenerateRareVendorItem(true, 1023, 1024));
	assert(PSh_Items_ShouldGenerateRareVendorItem(true, 1024, 1024));
	assert(PSh_Items_ShouldGenerateRareVendorItem(true, 17, 1));

	return 0;
}
