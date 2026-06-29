#include <plugin-shared.h>

D2RLOADER_PLUGIN_EXPORT D2UnitStrc** SUNIT_GetUnitList(D2UnitType nUnitType, D2GameStrc* pGame, uint32_t nUnitGuid)
{

}

D2RLOADER_PLUGIN_EXPORT D2UnitStrc* SUNIT_GetServerUnit(D2GameStrc* pGame, D2UnitType targetType, uint32_t targetGuid)
{
	D2UnitStrc** ppUnitList = nullptr;

	if (targetType == D2UnitType::Tile)
	{

	}
	else
	{
		ppUnitList = SUNIT_GetUnitList(targetType, pGame, targetGuid);
	}

	for (D2UnitStrc* pUnit = *ppUnitList; pUnit; pUnit = pUnit->)
	{
		if (pUnit->dwUnitId == targetGuid)
		{
			return pUnit;
		}
	}

	return nullptr;
}

D2RLOADER_PLUGIN_EXPORT void PATH_RefreshPath(D2GameStrc* pGame, D2UnitStrc* pUnit)
{
	if (!pGame || !pUnit || !pUnit->pDynamicPath || !pUnit->pDynamicPath->pTargetUnit)
	{
		return;
	}

	D2UnitStrc* pServerUnit = SUNIT_GetServerUnit(pGame, pUnit->pDynamicPath->targetType, pUnit->pDynamicPath->targetGuid);
	if (pServerUnit != pUnit->pDynamicPath->pTargetUnit)
	{
		pUnit->pDynamicPath->pTargetUnit = nullptr;
	}
	else if (pServerUnit->dwUnitType == D2UnitType::Item &&
		pServerUnit->dwAnimMode - 1 < 2)
	{	// FIXME use correct enum values
		pUnit->pDynamicPath->pTargetUnit = nullptr;
	}
}