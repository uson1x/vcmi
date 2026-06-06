/*
 * BattleHex.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>
#include "../../../lib/battle/BattleHex.h"
#include "../../../lib/battle/BattleHexArray.h"
#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class BattleHexProxy : public CopyableWrapper<const BattleHex, BattleHexProxy>
{
public:
	static constexpr std::string_view luaName = "BattleHex";
	static constexpr std::string_view luaDescription =
		"A single tile on the battlefield grid. Identified by a packed coordinate; methods on "
		"this type compute geometric relationships (distance, neighbours, line-of-sight target "
		"on a given side) without going through the battle callback.";

	static void registerMethods(MethodRegistrar & R);

	static BattleHex getClosestTile(const BattleHex & self, BattleSide side, const BattleHexArray & hexes);
};

/// Maps the C++ BattleHex type to its Lua-facing name for signature derivation.
inline std::string luaTypeNameOf(LuaTypeNameTag<BattleHex>)
{
	return std::string(BattleHexProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
