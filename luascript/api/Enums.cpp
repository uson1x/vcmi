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

Enums::EnumMap<EHealLevel> Enums::exportHealLevel() const
{
	return {
		{ "heal", EHealLevel::HEAL },
		{ "resurrect", EHealLevel::RESURRECT },
		{ "overheal", EHealLevel::OVERHEAL },
	};
}

Enums::EnumMap<EHealPower> Enums::exportHealPower() const
{
	return {
		{ "oneBattle", EHealPower::ONE_BATTLE },
		{ "permanent", EHealPower::PERMANENT },
	};
}

Enums::EnumMap<ESpellCastProblem> Enums::exportSpellCastProblem() const
{
	return {
		{ "noHeroToCastSpell", ESpellCastProblem::NO_HERO_TO_CAST_SPELL},
		{ "castsPerTurnLimit", ESpellCastProblem::CASTS_PER_TURN_LIMIT},
		{ "noSpellbook", ESpellCastProblem::NO_SPELLBOOK},
		{ "heroDoesntKnowSpell", ESpellCastProblem::HERO_DOESNT_KNOW_SPELL},
		{ "notEnoughMana", ESpellCastProblem::NOT_ENOUGH_MANA},
		{ "advmapSpellInsteadOfBattleSpell", ESpellCastProblem::ADVMAP_SPELL_INSTEAD_OF_BATTLE_SPELL},
		{ "spellLevelLimitExceeded", ESpellCastProblem::SPELL_LEVEL_LIMIT_EXCEEDED},
		{ "noSpellsToDispel", ESpellCastProblem::NO_SPELLS_TO_DISPEL},
		{ "noAppropriateTarget", ESpellCastProblem::NO_APPROPRIATE_TARGET},
		{ "stackImmuneToSpell", ESpellCastProblem::STACK_IMMUNE_TO_SPELL},
		{ "wrongSpellTarget", ESpellCastProblem::WRONG_SPELL_TARGET},
		{ "ongoinTacticPhase", ESpellCastProblem::ONGOING_TACTIC_PHASE},
		{ "magicIsBlocked", ESpellCastProblem::MAGIC_IS_BLOCKED}
	};
}

Enums::EnumMap<spells::AimType> Enums::exportAimType() const
{
	return {
		{ "nothing", spells::AimType::NOTHING },
		{ "creature", spells::AimType::CREATURE },
		{ "obstacle", spells::AimType::OBSTACLE },
		{ "location", spells::AimType::LOCATION },
	};
}

Enums::EnumMap<BonusDuration::BonusDuration> Enums::exportBonusDuration() const
{
	return {
		{ "permanent",                  BonusDuration::PERMANENT },
		{ "oneBattle",                  BonusDuration::ONE_BATTLE },
		{ "oneDay",                     BonusDuration::ONE_DAY },
		{ "oneWeek",                    BonusDuration::ONE_WEEK },
		{ "nTurns",                     BonusDuration::N_TURNS },
		{ "nDays",                      BonusDuration::N_DAYS },
		{ "untilBeingAttacked",         BonusDuration::UNTIL_BEING_ATTACKED },
		{ "untilAttack",                BonusDuration::UNTIL_ATTACK },
		{ "stackGetsTurn",              BonusDuration::STACK_GETS_TURN },
		{ "commanderKilled",            BonusDuration::COMMANDER_KILLED },
		{ "untilOwnAttack",             BonusDuration::UNTIL_OWN_ATTACK },
		{ "untilTakingIndirectDamage",  BonusDuration::UNTIL_TAKING_INDIRECT_DAMAGE },
		{ "untilAfterAttackSequence",   BonusDuration::UNTIL_AFTER_ATTACK_SEQUENCE },
	};
}

Enums::EnumMap<CObstacleInstance::EObstacleType> Enums::exportObstacleType() const
{
	return {
		{ "usual",        CObstacleInstance::USUAL },
		{ "absolute",     CObstacleInstance::ABSOLUTE_OBSTACLE },
		{ "spellCreated", CObstacleInstance::SPELL_CREATED },
		{ "moat",         CObstacleInstance::MOAT },
	};
}

Enums::EnumMap<EWallPart> Enums::exportWallPart() const
{
	return {
		{ "invalid",     EWallPart::INVALID },
		{ "keep",        EWallPart::KEEP },
		{ "bottomTower", EWallPart::BOTTOM_TOWER },
		{ "bottomWall",  EWallPart::BOTTOM_WALL },
		{ "belowGate",   EWallPart::BELOW_GATE },
		{ "overGate",    EWallPart::OVER_GATE },
		{ "upperWall",   EWallPart::UPPER_WALL },
		{ "upperTower",  EWallPart::UPPER_TOWER },
		{ "gate",        EWallPart::GATE },
	};
}

Enums::EnumMap<BattleSide> Enums::exportBattleSide() const
{
	return {
		{ "none",       BattleSide::NONE },
		{ "attacker",   BattleSide::ATTACKER },
		{ "defender",   BattleSide::DEFENDER },
	};
}

Enums::EnumMap<BonusSource> Enums::exportBonusSource() const
{
	return {
		{ "artifact",         BonusSource::ARTIFACT },
		{ "artifactInstance", BonusSource::ARTIFACT_INSTANCE },
		{ "objectType",       BonusSource::OBJECT_TYPE },
		{ "objectInstance",   BonusSource::OBJECT_INSTANCE },
		{ "creatureAbility",  BonusSource::CREATURE_ABILITY },
		{ "terrainNative",    BonusSource::TERRAIN_NATIVE },
		{ "terrainOverlay",   BonusSource::TERRAIN_OVERLAY },
		{ "spellEffect",      BonusSource::SPELL_EFFECT },
		{ "townStructure",    BonusSource::TOWN_STRUCTURE },
		{ "heroBaseSkill",    BonusSource::HERO_BASE_SKILL },
		{ "secondarySkill",   BonusSource::SECONDARY_SKILL },
		{ "heroSpecial",      BonusSource::HERO_SPECIAL },
		{ "army",             BonusSource::ARMY },
		{ "campaignBonus",    BonusSource::CAMPAIGN_BONUS },
		{ "stackExperience",  BonusSource::STACK_EXPERIENCE },
		{ "commander",        BonusSource::COMMANDER },
		{ "global",           BonusSource::GLOBAL },
		{ "other",            BonusSource::OTHER },
	};
}

Enums::EnumMap<BonusValueType> Enums::exportBonusValueType() const
{
	return {
		{ "additiveValue",        BonusValueType::ADDITIVE_VALUE },
		{ "baseNumber",           BonusValueType::BASE_NUMBER },
		{ "percentToAll",         BonusValueType::PERCENT_TO_ALL },
		{ "percentToBase",        BonusValueType::PERCENT_TO_BASE },
		{ "percentToSource",      BonusValueType::PERCENT_TO_SOURCE },
		{ "percentToTargetType",  BonusValueType::PERCENT_TO_TARGET_TYPE },
		{ "independentMax",       BonusValueType::INDEPENDENT_MAX },
		{ "independentMin",       BonusValueType::INDEPENDENT_MIN },
	};
}

}

VCMI_LIB_NAMESPACE_END
