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
#include "../MethodRegistrar.h"
#include "../library/Bonus.h"
#include "UnitState.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class UnitProxy : public RawPointerWrapper<const ::battle::Unit, UnitProxy>
{
public:
	static constexpr std::string_view luaName = "Unit";

	static void registerMethods(MethodRegistrar & R);

	static const Creature * getCreature(const ::battle::Unit & unit);
	static BattleHexArray getHexes(const ::battle::Unit & unit);
	static bool hasAbsoluteImmunity(const ::battle::Unit & unit, const spells::Spell & spell);
	static LuaUnitState copy(const ::battle::Unit & unit);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<::battle::Unit>)
{
	return std::string(UnitProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
