/*
 * SpellObstacleDescriptor.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/ApiTags.h>

#include "../../../lib/battle/BattleHex.h"
#include "../../../lib/battle/BattleSide.h"
#include "../../../lib/battle/CObstacleInstance.h"
#include "../../../lib/constants/EntityIdentifiers.h"

VCMI_LIB_NAMESPACE_BEGIN

struct SpellCreatedObstacle;

namespace scripting::api
{

/// POD descriptor for a spell-created obstacle passed from Lua scripts.
struct SpellObstacleDescriptor final : ApiSerializable<SpellObstacleDescriptor>
{
	si32 pos = BattleHex::INVALID;
	CObstacleInstance::EObstacleType obstacleType = CObstacleInstance::SPELL_CREATED;
	si32 spellIndex = -1;
	int32_t turnsRemaining = -1;
	int32_t casterSpellPower = 0;
	int32_t spellLevel = 0;
	BattleSide casterSide = BattleSide::ATTACKER;
	int32_t minimalDamage = 0;

	bool hidden = false;
	bool passable = false;
	bool trap = false;
	bool removeOnTrigger = false;
	bool nativeVisible = true;

	std::string trigger;
	std::string appearSound;
	std::string appearAnimation;
	std::string animation;

	std::vector<si16> customSize;

	/// Materializes a SpellCreatedObstacle ready to be sent through the battle pack.
	SpellCreatedObstacle toObstacle() const;

	template<typename Serializer>
	void serializeScript(Serializer & s)
	{
		s("pos",              pos);
		s("obstacleType",     obstacleType);
		s("spellIndex",       spellIndex);
		s("turnsRemaining",   turnsRemaining);
		s("casterSpellPower", casterSpellPower);
		s("spellLevel",       spellLevel);
		s("casterSide",       casterSide);
		s("minimalDamage",    minimalDamage);
		s("hidden",           hidden);
		s("passable",         passable);
		s("trap",             trap);
		s("removeOnTrigger",  removeOnTrigger);
		s("nativeVisible",    nativeVisible);
		s("trigger",          trigger);
		s("appearSound",      appearSound);
		s("appearAnimation",  appearAnimation);
		s("animation",        animation);
		s("customSize",       customSize);
	}
};

}

VCMI_LIB_NAMESPACE_END
