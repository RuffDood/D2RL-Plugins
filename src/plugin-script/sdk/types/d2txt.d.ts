/**
 * A single row within either Armor.txt, Misc.txt or Weapons.txt.
 * (This record is shared by all three files.)
 */
export interface D2ItemsTxt {
    // TODO: code, normcode, ubercode, ultracode
    readonly pSpell: number;
    readonly state: number;
    readonly stats: number[];
    readonly calc: number[];
    readonly len: number;
}

/**
 * A single row within the Levels.txt file
 */

/**
 * A single row within the Objects.txt file
 */

/**
 * A single row within the Skills.txt file
 */
export interface D2SkillsTxt {
    readonly skillId: number;
    readonly flags: number;
    readonly charclass: number;
    readonly monanim: number;
    readonly seqtrans: number;
    readonly seqnum: number;
    readonly range: number;
    readonly selectProc: number;
    readonly seqinput: number;

    /** itypea1, itypea2, itypea3 */
    readonly itypea: number[];
    /** itypeb1, itypeb2, itypeb3 */
    readonly itypeb: number[];
    /** etypea1, etypea2 */
    readonly etypea: number[];
    /** etypeb1, etypeb2 */
    readonly etypeb: number[];

    readonly srvstfunc: number;
    readonly srvdofunc: number;
    /** srvprgfunc1, srvprgfunc2, srvprgfunc3 */
    readonly srvprgfunc: number[];
    /** prgcalc1, prgcalc2, prgcalc3 */
    readonly prgcalc: number[];
    readonly prgdam: number;

    readonly srvmissile: number;
    readonly srvmissilea: number;
    readonly srvmissileb: number;
    readonly srvmissilec: number;
    readonly srvoverlay: number;

    readonly auraFilter: number;
    /** aurastat1–aurastat6 */
    readonly auraStat: number[];
    readonly auraLenCalc: number;
    readonly auraRangeCalc: number;
    /** aurastatcalc1–aurastatcalc6 */
    readonly auraStatCalc: number[];
    readonly aurastate: number;
    readonly auraTargetState: number;
    /** auraevent1–auraevent4 */
    readonly auraevent: number[];
    /** auraeventfunc1–auraeventfunc4 */
    readonly auraeventfunc: number[];

    readonly passivestate: number;
    readonly passiveitype: number;
    readonly passivereqweaponcount: number;
    /** passivestat1–passivestat14 */
    readonly passivestat: number[];
    /** passivecalc1–passivecalc14 */
    readonly passivecalc: number[];

    readonly summon: number;
    readonly pettype: number;
    readonly summode: number;
    readonly petmax: number;
    /** sumskill1–sumskill5 */
    readonly sumskill: number[];
    /** sumsk1calc–sumsk5calc */
    readonly sumsk_calc: number[];
    readonly sumumod: number;
    readonly sumoverlay: number;

    readonly cltmissile: number;
    readonly cltmissilea: number;
    readonly cltmissileb: number;
    readonly cltmissilec: number;
    readonly cltmissiled: number;
    readonly cltstfunc: number;
    readonly cltdofunc: number;
    /** cltprgfunc1–cltprgfunc3 */
    readonly cltprgfunc: number[];
    /** cltcalc1–cltcalc3 */
    readonly cltcalc: number[];

    readonly stsound: number;
    readonly stsoundclass: number;
    readonly dosound: number;
    readonly dosound_a: number;
    readonly dosound_b: number;
    readonly castoverlay: number;
    readonly tgtoverlay: number;
    readonly tgtsound: number;
    readonly prgoverlay: number;
    readonly prgsound: number;
    readonly cltoverlaya: number;
    readonly cltoverlayb: number;

    readonly itemTarget: number;
    readonly itemCastSound: number;
    readonly itemCastOverlay: number;

    readonly perdelay: number;
    readonly maxlvl: number;
    readonly resultFlags: number;
    readonly hitFlags: number;
    readonly hitClass: number;

    /** calc1–calc10 */
    readonly calc: number[];
    /** param1–param20 */
    readonly param: number[];

    readonly weapsel: number;
    readonly itemEffect: number;
    readonly itemCltEffect: number;

    readonly skpoints: number;
    readonly reqlevel: number;
    readonly reqstr: number;
    readonly reqdex: number;
    readonly reqint: number;
    readonly reqvit: number;
    /** reqskill1–reqskill3 */
    readonly reqskill: number[];

    readonly startmana: number;
    readonly minmana: number;
    readonly manashift: number;
    readonly mana: number;
    readonly lvlmana: number;
    readonly prgchargestocast: number;
    readonly prgchargesconsumed: number;
    readonly attackrank: number;
    readonly lineofsight: number;

    readonly globalDelay: number;
    readonly localdelay: number;
    readonly skilldesc: number;

    readonly toHit: number;
    readonly levToHit: number;
    readonly toHitCalc: number;
    readonly hitShift: number;
    readonly srcDam: number;

    readonly minDam: number;
    readonly maxDam: number;
    /** minlevdam1–minlevdam5 */
    readonly minLevDam: number[];
    /** maxlevdam1–maxlevdam5 */
    readonly maxLevDam: number[];
    readonly dmgSymPerCalc: number;

    readonly eType: number;
    readonly eMinDam: number;
    readonly eMaxDam: number;
    /** eminlev1–eminlev5 */
    readonly eMinLev: number[];
    /** emaxlev1–emaxlev5 */
    readonly eMaxLev: number[];
    readonly eDmgSymPerCalc: number;
    readonly eLevLen: number;
    readonly eLevLen1: number;
    readonly eLevLen2: number;
    readonly eLevLen3: number;
    readonly eLenSymPerCalc: number;

    readonly restrict: number;
    /** state1–state3 */
    readonly state: number[];
    readonly aitype: number;
    readonly aibonus: number;
    readonly cost_mult: number;
    readonly cost_add: number;
    readonly useServerMissilesOnRemoteClients: number;
    readonly srvstopfunc: number;
    readonly cltstopfunc: number;
}

/**
 * A single row within the TreasureClassEx.txt file
 */
export interface D2TreasureClassExTxt {

}