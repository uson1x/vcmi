/*
 * Unit.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>
#include "../../../lib/battle/Unit.h"
#include <vcmi/spells/Spell.h>

#include "../../LuaWrapper.h"
#include "../library/Bonus.h"
#include "UnitState.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class UnitProxy : public RawPointerWrapper<const ::battle::Unit, UnitProxy>
{
public:
	using Wrapper = RawPointerWrapper<const ::battle::Unit, UnitProxy>;
	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;

	static const Creature * getCreature(const ::battle::Unit *);
	static BattleHexArray getHexes(const ::battle::Unit *);
	static bool hasAbsoluteImmunity(const ::battle::Unit * unit, const spells::Spell * spell);
	static LuaUnitState copy(const ::battle::Unit * unit);
	static int getBonuses(lua_State * L);
};

}

VCMI_LIB_NAMESPACE_END
