import { D2ItemsTxt, D2SkillsTxt } from "./d2txt";

/** Callback for pSpell overrides. Return true to allow normal processing; false to cancel. */
export type pspellCallback = (game: D2Game, caster: D2Unit, item: D2Unit) => boolean;

/**
 * The global `trap` object — all plugin-script host bindings live here.
 * Available in every mod script without any import.
 */
declare const trap: {
    // ── Logging ──────────────────────────────────────────────────────────────

    logInfo(message: string): void;
    logWarn(message: string): void;
    logError(message: string): void;

    // ── TXT getters ───────────────────────────────────────────────────────────
    getItemsRecord(idx: number): D2ItemsTxt | undefined;
    getSkillsRecord(idx: number): D2SkillsTxt | undefined;

    // ── Callback registration ─────────────────────────────────────────────────
    // Most functions take an `index` that maps to a field in the game's .txt data
    // (e.g. missiles.txt wSrvDoFunc, skills.txt sfunc/stfunc, objects.txt InitFn).
    // Indices beyond the vanilla table size are supported for data-modded types.
    // setPSpell is the exception: its index is the misc.txt pSpell column value.

    /** Override the missile server DO function at dispatch index `index`. */
    setMissileDo(index: number, fn: (game: D2Game, missile: D2Unit) => number): void;

    /** Override the missile server HIT function at dispatch index `index`. */
    setMissileHit(index: number, fn: (game: D2Game, missile: D2Unit, target: D2Unit) => number): void;

    /** Override the missile server DMG function at dispatch index `index`. */
    setMissileDmg(index: number, fn: (game: D2Game, missile: D2Unit, target: D2Unit) => void): void;

    /** Override the skill server START function at dispatch index `index`. */
    setSkillStart(index: number, fn: (game: D2Game, unit: D2Unit, skillId: number, level: number) => number): void;

    /** Override the skill server DO function at dispatch index `index`. */
    setSkillDo(index: number, fn: (game: D2Game, unit: D2Unit, skillId: number, level: number) => number): void;

    /** Override the object INIT function at dispatch index `index`. */
    setObjInit(index: number, fn: (game: D2Game, obj: D2Unit) => void): void;

    /** Override the object OPERATE function at dispatch index `index`. */
    setObjOperate(index: number, fn: (game: D2Game, obj: D2Unit, player: D2Unit, param: number) => number): void;

    /** Override the object POPULATE function at dispatch index `index`. */
    setObjPopulate(index: number, fn: (game: D2Game, obj: D2Unit) => void): void;

    /**
     * Override the pSpell identified by `pSpellId` — the value from the misc.txt `pSpell` column (0–19).
     * `stFunc` is the activation handler (called first; return false to cancel item use entirely).
     * `doFunc` is the effect function (called after stFunc succeeds; return value is advisory).
     * Pass `null` for either to clear an existing override without touching the other.
     */
    setPSpell(pSpellId: number, stFunc: pspellCallback | null, doFunc: pspellCallback | null): void;

};
