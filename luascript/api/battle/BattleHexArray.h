/*
 * BattleHexArray.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../../lib/battle/BattleHexArray.h"
#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class BattleHexArrayProxy : public CopyableWrapper<const BattleHexArray, BattleHexArrayProxy>
{
public:
	static constexpr std::string_view luaName = "BattleHexArray";

	using Wrapper = CopyableWrapper<const BattleHexArray, BattleHexArrayProxy>;
	static void registerMethods(MethodRegistrar & R);

	static int insert(lua_State * L);
	static int erase(lua_State * L);
	static BattleHex at(const BattleHexArray & hexes, int index);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<BattleHexArray>)
{
	return std::string(BattleHexArrayProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
