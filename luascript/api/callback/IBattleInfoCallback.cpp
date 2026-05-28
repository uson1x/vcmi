/*
 * IBattleInfoCallback.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "IBattleInfoCallback.h"
#include <vcmi/Entity.h>

#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"

#include "../../../lib/GameConstants.h"
#include "../../../lib/battle/Unit.h"
#include "../../../lib/battle/AccessibilityInfo.h"
#include "../../../lib/battle/CBattleInfoCallback.h"
#include "../../../lib/BattleFieldHandler.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{


bool IBattleInfoCallbackProxy::isAccessibleForUnit(const IBattleInfoCallback * object, const battle::Unit * unit, BattleHex hex)
{
	const auto * cb = dynamic_cast<const CBattleInfoCallback *>(object);
	if(!cb || !unit)
		return false;
	return cb->getAccessibility(unit).accessible(hex, unit);
}

bool IBattleInfoCallbackProxy::hasPenaltyOnLine(const IBattleInfoCallback * object, BattleHex from, BattleHex dest, bool checkWall, bool checkMoat)
{
	const auto * cb = dynamic_cast<const CBattleInfoCallback *>(object);
	if(!cb)
		return false;
	return cb->battleHasPenaltyOnLine(from, dest, checkWall, checkMoat);
}

const std::vector<IBattleInfoCallbackProxy::CustomRegType> IBattleInfoCallbackProxy::REGISTER_CUSTOM =
{
	{ "getNextUnitId",    LuaMethodWrapper<&BattleCb::battleNextUnitId>::invoke,   false },
	{ "getTacticDistance", LuaMethodWrapper<&BattleCb::battleTacticDist>::invoke, false },
	{ "getUnitById",      LuaMethodWrapper<&BattleCb::battleGetUnitByID>::invoke, false },
	{ "isFinished",       LuaMethodWrapper<&BattleCb::battleIsFinished>::invoke,  false },

	{ "getAvailableHex",      LuaCallWrapper<&IBattleInfoCallbackProxy::getAvailableHex>::invoke, false },
	{ "getAnyUnitIf",         LuaCallWrapper<&IBattleInfoCallbackProxy::getAnyUnitIf>::invoke,    false },
	{ "isAccessibleForUnit",  LuaFunctionWrapper<&IBattleInfoCallbackProxy::isAccessibleForUnit>::invoke, false },
	{ "hasPenaltyOnLine",     LuaFunctionWrapper<&IBattleInfoCallbackProxy::hasPenaltyOnLine>::invoke,    false },
};

int IBattleInfoCallbackProxy::getAvailableHex(lua_State * L)
{
	LuaStack S(L);

	const IBattleInfoCallback * object;
	S.getNonNull(1, object);

	const Creature * creature;
	BattleSide side;
	si16 hexVal = BattleHex::INVALID;

	S.get(2, creature);
	S.get(3, side);

	if(lua_gettop(L) >= 4 && !lua_isnil(L, 4))
		S.get(4, hexVal);

	S.clear();
	BattleHex result = object->getAvailableHex(creature, side, hexVal);
	S.push(result);
	return 1;
}

int IBattleInfoCallbackProxy::getAnyUnitIf(lua_State * L)
{
	LuaStack S(L);

	const IBattleInfoCallback * object;
	S.getNonNull(1, object);

	if(!S.isFunction(2))
		throw LuaApiException("Invalid parameters passed into getAnyUnitIf!");

	battle::Units units = object->battleGetUnitsIf([&L](const battle::Unit * unit){
		LuaStack S2(L);
		lua_pushvalue(L, 2); // bring copy of the function to top of stack
		S2.push(unit);

		if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
			std::string error = lua_tostring(L, S2.absindex(-1));
			throw LuaApiException("Lua getAnyUnitIf callback failed with message: " + error);
		}

		bool result = lua_toboolean(L, S2.absindex(-1)) != 0;
		S2.restoreInitialTop();
		return result;
	});

	S.clear();

	if (!units.empty())
		S.push(units.front());
	else
		S.pushNil();

	return 1;
}

}

VCMI_LIB_NAMESPACE_END
