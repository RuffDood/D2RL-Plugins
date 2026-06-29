/** Object animation modes (D2C_ObjModes). Used with unit.animMode and unit.setAnimMode(). */
export const enum ObjMode {
    Neutral = 0, // NU
    Operating = 1, // OP
    Opened = 2, // ON
    Special1 = 3, // S1
    Special2 = 4, // S2
    Special3 = 5, // S3
    Special4 = 6, // S4
    Special5 = 7, // S5
}

/** Unit type values returned by D2Unit.unitType */
export const enum UnitType {
    Player = 0,
    Monster = 1,
    ObjectType = 2,
    Missile = 3,
    Item = 4,
    Tile = 5,
}

/** Difficulty values returned by D2Game.difficulty */
export const enum Difficulty {
    Normal = 0,
    Nightmare = 1,
    Hell = 2,
}

/**
 * Common stat IDs for D2Unit.getStat().
 * Add more as needed from StatList.txt / ItemStatCost.txt.
 */
export const enum StatId {
    Strength = 0,
    Energy = 1,
    Dexterity = 2,
    Vitality = 3,
    StatPoints = 4,
    SkillPoints = 5,
    Life = 6,
    MaxLife = 7,
    Mana = 8,
    MaxMana = 9,
    Stamina = 10,
    MaxStamina = 11,
    Level = 12,
    Experience = 13,
    Gold = 14,
    GoldBank = 15,
    EnhancedDefPct = 16,
    EnhancedMaxDmgPct = 17,
    EnhancedMinDmgPct = 18,
    AttackRating = 19,
    BlockRating = 20,
    MinDamage = 21,
    MaxDamage = 22,
    Defense = 31,
    PhysResist = 36,
    FireResist = 39,
    ColdResist = 43,
    LightResist = 41,
    PoisonResist = 45,
    IncreasedAttackSpeed = 93,
    FasterRunWalk = 96,
    State = 98,
    FasterHitRecovery = 99,
    PlayerCount = 100,
    FasterCastRate = 105,
}
