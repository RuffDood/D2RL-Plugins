/**
 * Wraps a native D2UnitStrc pointer.
 * Valid only for the duration of the callback that received it — do not store
 * across callback boundaries.
 */
interface D2Unit {
    /** D2UnitType value (0=Player, 1=Monster, 2=Object, 3=Missile, 4=Item) */
    readonly unitType: number;
    /** Class-specific flags word at offset +0x04 */
    readonly unitFlags: number;
    /**
     * Animation / object / missile mode. For objects, maps to D2C_ObjModes
     * (0 = Neutral, 1 = Operating, 2 = Opened, ...).
     */
    readonly animMode: number;
    /** RNG seed low word */
    readonly seedLow: number;
    /** RNG seed high word */
    readonly seedHigh: number;
    /** General flags at offset +0x124 */
    readonly flags: number;
    /** Gets the items.txt record (if this is an item) */
    readonly itemsRecord: number;

    /** Returns the current value of the given stat (wraps PSh_GetStat). */
    getStat(statId: number): number;

    /**
     * Advances the unit's RNG seed and returns a random value.
     * @param range If provided, returns value in [0, range).
     */
    roll(range?: number): number;

    /**
     * Changes the unit's animation/object mode. For objects, use ObjMode constants.
     * Wraps UNITS_ChangeAnimMode.
     */
    setAnimMode(mode: number): void;

    /**
     * Starts running a skill.
     * Wrappper for D2GAME_Skills_Handler.
     */
    runSkill(game: D2Game, skillId: number, skillLevel: number, consumeMana: boolean,
        fromItem: boolean, useTarget: boolean
    ): void;

    /**
     * Evaluates a skill formula (calc expression) from Skills.txt in the context
     * of this unit.  Pass the raw calc value from a {@link D2SkillsTxt} record
     * (e.g. `rec.calc[0]`) and the skill's ID.
     * Wraps SKILLS_EvaluateSkillFormula.
     * @param game   The current game (supplies the expansion flag for the data table).
     * @param calcIdx  The calc formula index (nCalc field value from Skills.txt).
     * @param skillId  The skill ID.
     * @returns The integer result of the formula.
     */
    evaluateSkillCalc(game: D2Game, calcIdx: number, skillId: number): number;
}

/**
 * Wraps a native D2GameStrc pointer.
 * Valid only for the duration of the callback that received it.
 */
interface D2Game {
    /** 0 = Normal, 1 = Nightmare, 2 = Hell */
    readonly difficulty: number;
    /** Whether the game is an expansion (LoD) game */
    readonly expansion: boolean;
}
