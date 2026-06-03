/*
 * BattleHex.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleHex.h"

#include "../../../lib/GameLibrary.h"
#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"
#include "../Registry.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

const std::vector<BattleHexProxy::CustomRegType> BattleHexProxy::REGISTER_CUSTOM =
{
	{"isValid",         LuaMethodWrapper<&BattleHex::isValid>::invoke,               false},
	{"isAvailable",     LuaMethodWrapper<&BattleHex::isAvailable>::invoke,           false},
	{"toInteger",       LuaMethodWrapper<&BattleHex::toInt>::invoke,                 false},
	{"getClosestTile",  LuaFunctionWrapper<&BattleHexProxy::getClosestTile>::invoke, false},
	{"copyToNorthWest", LuaMethodWrapper<&BattleHex::copyToNorthWest>::invoke,       false},
	{"copyToNorthEast", LuaMethodWrapper<&BattleHex::copyToNorthEast>::invoke,       false},
	{"copyToEast",      LuaMethodWrapper<&BattleHex::copyToEast>::invoke,            false},
	{"copyToSouthEast", LuaMethodWrapper<&BattleHex::copyToSouthEast>::invoke,       false},
	{"copyToSouthWest", LuaMethodWrapper<&BattleHex::copyToSouthWest>::invoke,       false},
	{"copyToWest",      LuaMethodWrapper<&BattleHex::copyToWest>::invoke,            false},
};

BattleHex BattleHexProxy::getClosestTile(const BattleHex & self, BattleSide side, const BattleHexArray & hexes)
{
	return BattleHex::getClosestTile(side, self, hexes);
}

}

VCMI_LIB_NAMESPACE_END
