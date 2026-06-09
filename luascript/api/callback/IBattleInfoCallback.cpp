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

#include "../Enums.h"
#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"

// Proxy headers brought in for their luaTypeNameOf ADL overloads — the signature
// derivation in MethodRegistrar::function<>() needs those visible at the call site.
#include "../battle/BattleHex.h"
#include "../battle/BattleHexArray.h"
#include "../battle/Obstacle.h"
#include "../battle/Unit.h"

#include "../../../lib/GameConstants.h"
#include "../../../lib/battle/Unit.h"
#include "../../../lib/battle/AccessibilityInfo.h"
#include "../../../lib/battle/CBattleInfoCallback.h"
#include "../../../lib/battle/CBattleInfoEssentials.h"
#include "../../../lib/battle/CObstacleInstance.h"
#include "../../../lib/BattleFieldHandler.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

void IBattleInfoCallbackProxy::registerMethods(MethodRegistrar & R)
{
	R.method<&BattleCb::battleTacticDist>("getTacticDistance",
		"Returns the available tactic phase distance, or 0 if the tactic phase has ended.");
	R.cfunction<&IBattleInfoCallbackProxy::getAvailableHex>("getAvailableHex",
		"(creature: Creature, side: BattleSide, hex: BattleHex?): BattleHex",
		"Returns an empty hex next to desired location that the creature can be placed on.");
	R.cfunction<&IBattleInfoCallbackProxy::getUnitsIf>("getUnitsIf",
		"(predicate: fun(u: Unit): boolean): Unit[]",
		"Returns all units for which the predicate returns true.");
	R.function<&IBattleInfoCallbackProxy::isAccessibleForUnit>("isAccessibleForUnit",
		"True if the given hex is reachable by the given unit either on current turn or on any future turns.");
	R.function<&IBattleInfoCallbackProxy::hasPenaltyOnLine>("hasPenaltyOnLine",
		"True if a ranged attack along this line crosses a wall or moat (per the flags).");
	R.function<&IBattleInfoCallbackProxy::getUnitByPos>("getUnitByPos",
		"Returns the unit covering the given hex, or nil. Pass true to ignore dead units.");
	R.function<&IBattleInfoCallbackProxy::getAllObstacles>("getAllObstacles",
		"Returns all obstacles on the battlefield.");
	R.function<&IBattleInfoCallbackProxy::getObstaclesOnPos>("getObstaclesOnPos",
		"Returns the obstacles on the given hex. Pass true to limit to blocking obstacles.");
	R.function<&IBattleInfoCallbackProxy::hasFortifications>("hasFortifications",
		"True if the battle is a siege with fortifications present.");
	R.function<&IBattleInfoCallbackProxy::hasMoat>("hasMoat",
		"True if the battlefield has a moat.");
	R.function<&IBattleInfoCallbackProxy::hasNativeStack>("hasNativeStack",
		"True if the given side has at least one native-terrain stack.");
	R.function<&IBattleInfoCallbackProxy::getAllPossibleHexes>("getAllPossibleHexes",
		"Returns every valid battlefield hex.");
	R.function<&IBattleInfoCallbackProxy::getWallState>("getWallState",
		"Returns the current state of the given wall section, or nil if absent.");
	R.function<&IBattleInfoCallbackProxy::isWallPartAttackable>("isWallPartAttackable",
		"True if the given wall section can be targeted by an attack.");
	R.function<&IBattleInfoCallbackProxy::wallPartToBattleHex>("wallPartToBattleHex",
		"Returns the battle hex corresponding to the given wall section.");
	R.function<&IBattleInfoCallbackProxy::hexToWallPart>("hexToWallPart",
		"Returns the wall section corresponding to the given battle hex.");
	R.function<&IBattleInfoCallbackProxy::getTowerShooterHex>("getTowerShooterHex",
		"Returns the hex used by the tower shooter for the given wall section.");
}

bool IBattleInfoCallbackProxy::isAccessibleForUnit(const IBattleInfoCallback & object, const battle::Unit & unit, BattleHex hex)
{
	const auto * cb = dynamic_cast<const CBattleInfoCallback *>(&object);
	if(!cb)
		return false;
	return cb->getAccessibility(&unit).accessible(hex, &unit);
}

bool IBattleInfoCallbackProxy::hasPenaltyOnLine(const IBattleInfoCallback & object, BattleHex from, BattleHex dest, bool checkWall, bool checkMoat)
{
	const auto * cb = dynamic_cast<const CBattleInfoCallback *>(&object);
	if(!cb)
		return false;
	return cb->battleHasPenaltyOnLine(from, dest, checkWall, checkMoat);
}

int IBattleInfoCallbackProxy::getAvailableHex(lua_State * L)
{
	LuaStack S(L);

	const IBattleInfoCallback * object;
	S.getNonNull(1, object);

	const Creature * creature;
	BattleSide side;
	BattleHex hexVal;

	S.get(2, creature);
	S.get(3, side);

	if(lua_gettop(L) >= 4 && !lua_isnil(L, 4))
		S.get(4, hexVal);

	S.clear();
	BattleHex result = object->getAvailableHex(creature, side, hexVal);
	S.push(result);
	return 1;
}

const battle::Unit * IBattleInfoCallbackProxy::getUnitByPos(const IBattleInfoCallback & object, BattleHex hex, bool onlyAlive)
{
	return object.battleGetUnitByPos(hex, onlyAlive);
}

std::vector<std::shared_ptr<const CObstacleInstance>> IBattleInfoCallbackProxy::getAllObstacles(const IBattleInfoCallback & object)
{
	return object.battleGetAllObstacles(BattleSide::ALL_KNOWING);
}

std::vector<std::shared_ptr<const CObstacleInstance>> IBattleInfoCallbackProxy::getObstaclesOnPos(const IBattleInfoCallback & object, BattleHex hex, bool onlyBlocking)
{
	return object.battleGetAllObstaclesOnPos(hex, onlyBlocking);
}

bool IBattleInfoCallbackProxy::hasFortifications(const IBattleInfoCallback & object)
{
	return object.hasFortifications();
}

bool IBattleInfoCallbackProxy::hasMoat(const IBattleInfoCallback & object)
{
	return object.hasMoat();
}

bool IBattleInfoCallbackProxy::hasNativeStack(const IBattleInfoCallback & object, BattleSide side)
{
	return object.battleHasNativeStack(side);
}

BattleHexArray IBattleInfoCallbackProxy::getAllPossibleHexes(const IBattleInfoCallback &)
{
	BattleHexArray result;
	for(int i = 0; i < GameConstants::BFIELD_SIZE; i++)
		result.insert(BattleHex(static_cast<si16>(i)));
	return result;
}

std::optional<EWallState> IBattleInfoCallbackProxy::getWallState(const IBattleInfoCallback & object, EWallPart part)
{
	EWallState state = object.battleGetWallState(part);
	if(state == EWallState::NONE)
		return std::nullopt;
	return state;
}

bool IBattleInfoCallbackProxy::isWallPartAttackable(const IBattleInfoCallback & object, EWallPart part)
{
	return object.isWallPartAttackable(part);
}

BattleHex IBattleInfoCallbackProxy::wallPartToBattleHex(const IBattleInfoCallback & object, EWallPart part)
{
	return object.wallPartToBattleHex(part);
}

EWallPart IBattleInfoCallbackProxy::hexToWallPart(const IBattleInfoCallback & object, BattleHex hex)
{
	return object.battleHexToWallPart(hex);
}

BattleHex IBattleInfoCallbackProxy::getTowerShooterHex(const IBattleInfoCallback & object, EWallPart part)
{
	return object.getTowerShooterHex(part);
}

int IBattleInfoCallbackProxy::getUnitsIf(lua_State * L)
{
	LuaStack S(L);

	const IBattleInfoCallback * object;
	S.getNonNull(1, object);

	if(!S.isFunction(2))
		throw LuaApiException("Invalid parameters passed into getUnitsIf!");

	battle::Units units = object->battleGetUnitsIf([&L](const battle::Unit * unit){
		LuaStack S2(L);
		lua_pushvalue(L, 2); // bring copy of the function to top of stack
		S2.push(unit);

		if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
			std::string error = lua_tostring(L, S2.absindex(-1));
			throw LuaApiException("Lua getUnitsIf callback failed with message: " + error);
		}

		bool result = lua_toboolean(L, S2.absindex(-1)) != 0;
		S2.restoreInitialTop();
		return result;
	});

	S.clear();
	S.push(units);
	return 1;
}

}

VCMI_LIB_NAMESPACE_END
