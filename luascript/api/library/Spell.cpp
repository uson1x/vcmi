/*
 * api/Spell.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Spell.h"

#include "../Registry.h"
#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api::library
{

using ::spells::Spell;

const std::vector<SpellProxy::CustomRegType> SpellProxy::REGISTER_CUSTOM =
{
	{"getIconIndex",        LuaMethodWrapper<&Entity::getIconIndex, Spell>::invoke,      false},
	{"getIndex",            LuaMethodWrapper<&Entity::getIndex, Spell>::invoke,          false},
	{"getJsonKey",          LuaMethodWrapper<&Entity::getJsonKey, Spell>::invoke,        false},
	{"getName",             LuaMethodWrapper<&Entity::getNameTranslated, Spell>::invoke, false},

	{"isAdventure",         LuaMethodWrapper<&Spell::isAdventure>::invoke,               false},
	{"isCombat",            LuaMethodWrapper<&Spell::isCombat>::invoke,                  false},
	{"isCreatureAbility",   LuaMethodWrapper<&Spell::isCreatureAbility>::invoke,         false},
	{"isPositive",          LuaMethodWrapper<&Spell::isPositive>::invoke,                false},
	{"isNegative",          LuaMethodWrapper<&Spell::isNegative>::invoke,                false},
	{"isNeutral",           LuaMethodWrapper<&Spell::isNeutral>::invoke,                 false},
	{"isDamage",            LuaMethodWrapper<&Spell::isDamage>::invoke,                  false},
	{"isOffensive",         LuaMethodWrapper<&Spell::isOffensive>::invoke,               false},
	{"isSpecial",           LuaMethodWrapper<&Spell::isSpecial>::invoke,                 false},
	{"isPersistent",        LuaMethodWrapper<&Spell::isPersistent>::invoke,              false},

	{"getCost",             LuaMethodWrapper<&Spell::getCost>::invoke,                   false},
	{"getBasePower",        LuaMethodWrapper<&Spell::getBasePower>::invoke,              false},
	{"getLevelPower",       LuaMethodWrapper<&Spell::getLevelPower>::invoke,             false},
	{"getLevelDescription", LuaMethodWrapper<&Spell::getDescriptionTranslated>::invoke,  false},
};

}

VCMI_LIB_NAMESPACE_END
