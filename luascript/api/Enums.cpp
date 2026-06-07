/*
 * Enums.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Enums.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

EnumGroup<EHealLevel> Enums::exportHealLevel() const
{
	return {{
		{ "heal",      EHealLevel::HEAL,      "Standard healing — capped at the stack's starting count, does not revive dead." },
		{ "resurrect", EHealLevel::RESURRECT, "Brings dead creatures in the stack back, up to the starting count." },
		{ "overheal",  EHealLevel::OVERHEAL,  "Allows healing past the starting count (e.g. summoned reinforcements)." },
	}};
}

EnumGroup<EHealPower> Enums::exportHealPower() const
{
	return {{
		{ "oneBattle", EHealPower::ONE_BATTLE, "Healing wears off when the battle ends." },
		{ "permanent", EHealPower::PERMANENT,  "Healing persists into the post-battle stack state." },
	}};
}

EnumGroup<ESpellCastProblem> Enums::exportSpellCastProblem() const
{
	return {{
		{ "noHeroToCastSpell",              ESpellCastProblem::NO_HERO_TO_CAST_SPELL,                "There is no hero available to cast the spell." },
		{ "castsPerTurnLimit",              ESpellCastProblem::CASTS_PER_TURN_LIMIT,                 "The caster has already used their per-turn cast allowance." },
		{ "noSpellbook",                    ESpellCastProblem::NO_SPELLBOOK,                         "The caster does not carry a spellbook." },
		{ "heroDoesntKnowSpell",            ESpellCastProblem::HERO_DOESNT_KNOW_SPELL,               "The spell is not present in the caster's spellbook." },
		{ "notEnoughMana",                  ESpellCastProblem::NOT_ENOUGH_MANA,                      "The caster does not have enough mana for this spell." },
		{ "advmapSpellInsteadOfBattleSpell",ESpellCastProblem::ADVMAP_SPELL_INSTEAD_OF_BATTLE_SPELL, "An adventure-map spell was attempted in battle context." },
		{ "spellLevelLimitExceeded",        ESpellCastProblem::SPELL_LEVEL_LIMIT_EXCEEDED,           "The caster's wisdom is too low for the spell's level." },
		{ "noSpellsToDispel",               ESpellCastProblem::NO_SPELLS_TO_DISPEL,                  "Dispel was cast but there are no eligible spells to remove." },
		{ "noAppropriateTarget",            ESpellCastProblem::NO_APPROPRIATE_TARGET,                "No legal target was selected for the spell." },
		{ "stackImmuneToSpell",             ESpellCastProblem::STACK_IMMUNE_TO_SPELL,                "The chosen target stack is immune to this spell." },
		{ "wrongSpellTarget",               ESpellCastProblem::WRONG_SPELL_TARGET,                   "The selected target is not valid for this spell's AimType." },
		{ "ongoinTacticPhase",              ESpellCastProblem::ONGOING_TACTIC_PHASE,                 "Spells may not be cast during the tactic phase." },
		{ "magicIsBlocked",                 ESpellCastProblem::MAGIC_IS_BLOCKED,                     "Magic is suppressed on this battlefield (anti-magic terrain or effect)." },
	}};
}

EnumGroup<spells::AimType> Enums::exportAimType() const
{
	return {{
		{ "nothing",  spells::AimType::NOTHING,  "Spell takes no target (cast directly when selected)." },
		{ "creature", spells::AimType::CREATURE, "Spell targets a single unit on the battlefield." },
		{ "obstacle", spells::AimType::OBSTACLE, "Spell targets an existing obstacle." },
		{ "location", spells::AimType::LOCATION, "Spell targets a battlefield hex (no unit required)." },
	}};
}

EnumGroup<BonusDuration::BonusDuration> Enums::exportBonusDuration() const
{
	return {{
		{ "permanent",                  BonusDuration::PERMANENT,                   "Lasts forever (until explicitly removed)." },
		{ "oneBattle",                  BonusDuration::ONE_BATTLE,                  "Expires at the end of the current battle." },
		{ "oneDay",                     BonusDuration::ONE_DAY,                     "Expires after one in-game day." },
		{ "oneWeek",                    BonusDuration::ONE_WEEK,                    "Expires after one in-game week." },
		{ "nTurns",                     BonusDuration::N_TURNS,                     "Expires after N battle turns (N taken from the bonus' turns field)." },
		{ "nDays",                      BonusDuration::N_DAYS,                      "Expires after N in-game days." },
		{ "untilBeingAttacked",         BonusDuration::UNTIL_BEING_ATTACKED,        "Expires the first time the bearer is attacked." },
		{ "untilAttack",                BonusDuration::UNTIL_ATTACK,                "Expires the first time the bearer makes any attack." },
		{ "stackGetsTurn",              BonusDuration::STACK_GETS_TURN,             "Expires when the bearer's stack next gets to act." },
		{ "commanderKilled",            BonusDuration::COMMANDER_KILLED,            "Tied to a commander — expires when the commander dies." },
		{ "untilOwnAttack",             BonusDuration::UNTIL_OWN_ATTACK,            "Expires when the bearer initiates an attack (not counter-attack)." },
		{ "untilTakingIndirectDamage",  BonusDuration::UNTIL_TAKING_INDIRECT_DAMAGE,"Expires when the bearer takes spell or environmental damage." },
		{ "untilAfterAttackSequence",   BonusDuration::UNTIL_AFTER_ATTACK_SEQUENCE, "Expires after the current attack-and-counter sequence resolves." },
	}};
}

EnumGroup<CObstacleInstance::EObstacleType> Enums::exportObstacleType() const
{
	return {{
		{ "usual",        CObstacleInstance::USUAL,             "Regular battlefield obstacle (rock, tree, …)." },
		{ "absolute",     CObstacleInstance::ABSOLUTE_OBSTACLE, "Map-wide obstacle blocking entire tiles regardless of side." },
		{ "spellCreated", CObstacleInstance::SPELL_CREATED,     "Obstacle placed by a spell (Land Mine, Quicksand, Fire Wall, …)." },
		{ "moat",         CObstacleInstance::MOAT,              "Town-siege moat tile." },
	}};
}

EnumGroup<EWallPart> Enums::exportWallPart() const
{
	return {{
		{ "invalid",     EWallPart::INVALID,      "No wall part. Returned for hexes outside the fortifications." },
		{ "indestructiblePart",   EWallPart::INDESTRUCTIBLE_PART, "Sections of walls that always exist and always block ranged attacks" },
		{ "indestructibleGate",   EWallPart::INDESTRUCTIBLE_PART_OF_GATE, "Section of gate that always exists, but does not blocks ranged attacks" },
		{ "keep",        EWallPart::KEEP,         "Central keep building (siege defender's last line)." },
		{ "bottomTower", EWallPart::BOTTOM_TOWER, "Lower flanking tower of the town walls." },
		{ "bottomWall",  EWallPart::BOTTOM_WALL,  "Wall segment between the bottom tower and the gate." },
		{ "belowGate",   EWallPart::BELOW_GATE,   "Wall segment immediately below the gatehouse." },
		{ "overGate",    EWallPart::OVER_GATE,    "Wall segment immediately above the gatehouse." },
		{ "upperWall",   EWallPart::UPPER_WALL,   "Wall segment between the gate and the upper tower." },
		{ "upperTower",  EWallPart::UPPER_TOWER,  "Upper flanking tower of the town walls." },
		{ "gate",        EWallPart::GATE,         "Town gate (the destructible entry door)." },
	}};
}

EnumGroup<BattleSide> Enums::exportBattleSide() const
{
	return {{
		{ "none",     BattleSide::NONE,     "Not associated with any side (e.g. neutral obstacle)." },
		{ "attacker", BattleSide::ATTACKER, "The attacking army (the side that initiated the battle)." },
		{ "defender", BattleSide::DEFENDER, "The defending army." },
	}};
}

EnumGroup<BonusSource> Enums::exportBonusSource() const
{
	return {{
		{ "artifact",         BonusSource::ARTIFACT,          "Granted by an artifact definition (the type, not a worn instance)." },
		{ "artifactInstance", BonusSource::ARTIFACT_INSTANCE, "Granted by a specific equipped artifact instance." },
		{ "objectType",       BonusSource::OBJECT_TYPE,       "Granted by a map-object type (e.g. all Mystical Gardens)." },
		{ "objectInstance",   BonusSource::OBJECT_INSTANCE,   "Granted by a specific placed map object." },
		{ "creatureAbility",  BonusSource::CREATURE_ABILITY,  "Innate to the creature type (e.g. dragon breath, undead)." },
		{ "terrainNative",    BonusSource::TERRAIN_NATIVE,    "Native-terrain bonus (creature on its faction's home terrain)." },
		{ "terrainOverlay",   BonusSource::TERRAIN_OVERLAY,   "Battlefield-terrain overlay bonus (magic plains, fiery fields, …)." },
		{ "spellEffect",      BonusSource::SPELL_EFFECT,      "Active spell effect on the bearer." },
		{ "townStructure",    BonusSource::TOWN_STRUCTURE,    "Built town structure providing the bonus to the garrison." },
		{ "heroBaseSkill",    BonusSource::HERO_BASE_SKILL,   "Hero primary skill (attack/defense/power/knowledge)." },
		{ "secondarySkill",   BonusSource::SECONDARY_SKILL,   "Hero secondary skill level (Tactics, Offense, Wisdom, …)." },
		{ "heroSpecial",      BonusSource::HERO_SPECIAL,      "Hero specialty (creature affinity, spell affinity, etc.)." },
		{ "army",             BonusSource::ARMY,              "Army composition bonus (e.g. all-archers cohesion)." },
		{ "campaignBonus",    BonusSource::CAMPAIGN_BONUS,    "Carry-over bonus from a campaign scenario." },
		{ "stackExperience",  BonusSource::STACK_EXPERIENCE,  "Stack-experience rank bonus." },
		{ "commander",        BonusSource::COMMANDER,         "Commander unit ability." },
		{ "global",           BonusSource::GLOBAL,            "Engine-wide global effect (rules toggles, debug, …)." },
		{ "other",            BonusSource::OTHER,             "Source not represented by another category." },
	}};
}

EnumGroup<BonusValueType> Enums::exportBonusValueType() const
{
	return {{
		{ "additiveValue",        BonusValueType::ADDITIVE_VALUE,         "Adds directly to the base value." },
		{ "baseNumber",           BonusValueType::BASE_NUMBER,            "Replaces the base value before any additive/percent steps." },
		{ "percentToAll",         BonusValueType::PERCENT_TO_ALL,         "Percentage applied to the running total of all sources combined." },
		{ "percentToBase",        BonusValueType::PERCENT_TO_BASE,        "Percentage applied to the base value only." },
		{ "percentToSource",      BonusValueType::PERCENT_TO_SOURCE,      "Percentage applied to the same-source subtotal." },
		{ "percentToTargetType",  BonusValueType::PERCENT_TO_TARGET_TYPE, "Percentage applied only to bonuses from a matching targetSourceType." },
		{ "independentMax",       BonusValueType::INDEPENDENT_MAX,        "Independent ceiling — wins if greater than the accumulated value." },
		{ "independentMin",       BonusValueType::INDEPENDENT_MIN,        "Independent floor — wins if smaller than the accumulated value." },
	}};
}

}

VCMI_LIB_NAMESPACE_END
