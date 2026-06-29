import { ObjMode, UnitType } from '../types/d2ids';
import { trap } from '../types/trap';

// Replication of OBJECTS_OperateFunction13_TorchTiki (D2MOO/source/D2Game/src/OBJECTS/ObjMode.cpp).
// Toggles the object between its lit (Operating) and unlit (Neutral) state when a player clicks it.
// Any intermediate mode (Opened, Special1..5) is left unchanged by design — the original does the same.
// Set operateFn = 13 in objects.txt to apply this to an object, or use a custom index here.
trap.setObjOperate(13, (game, obj, player, param) => {
    if (obj.animMode !== ObjMode.Neutral) {
        if (obj.animMode === ObjMode.Operating) {
            obj.setAnimMode(ObjMode.Neutral);
        }
    } else {
        obj.setAnimMode(ObjMode.Operating);
    }
    return 1;
});

// Creates an entirely new pSpell - one that invokes a specific skill of a given skill index (`calc1`) 
// at `calc2` level. Since books.txt has been expanded, this allows us to create "Scroll of <Skill>" 
// items just like Diablo 1 had.
trap.setPSpell(16, 
    (pGame, pCaster, pItem) => true,
    (pGame, pCaster, pItem) => {
        const itemRecord = trap.getItemsRecord(pItem.itemsRecord);
        if (itemRecord === undefined) {
            trap.logError(`invalid item record on pSpell 16`);
            return false;
        }
        const skillsRecord = trap.getSkillsRecord(itemRecord.calc[0]);
        if (skillsRecord === undefined) {
            trap.logError(`invalid skill '${itemRecord.calc[0]}' called`);
            return false;
        }

        pCaster.runSkill(pGame, itemRecord.calc[0], itemRecord.calc[1], 
            false, true, true);
        return true;
    }
);

// An example of a skill dofunc override - this is replacing the Telekinesis DoFunc
trap.setSkillDo(21, (pGame, pCaster, skillId, skillLevel) => {
    const skill = trap.getSkillsRecord(skillId);
    const target:D2Unit = SUNIT_GETTARGETUNIT;
    pCaster.flags |= SkSrvDoFunc

    if (target.unitType === UnitType.Player || target.unitType === UnitType.Monster) {

    } else if (target.unitType === UnitType.Item) {

    } else if (target.unitType === UnitType.ObjectType) {
    }

    return 1;
});