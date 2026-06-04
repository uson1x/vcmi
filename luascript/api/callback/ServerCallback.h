/*
 * ServerCallback.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/ServerCallback.h>

#include "../../LuaWrapper.h"
#include "../../../lib/battle/BattleHex.h"
#include "../../../lib/bonuses/BonusList.h"
#include "../../../lib/json/JsonNode.h"
#include "../../../lib/constants/EntityIdentifiers.h"
#include "../../../lib/constants/Enumerations.h"

VCMI_LIB_NAMESPACE_BEGIN

struct CObstacleInstance;

namespace battle { class Unit; class UnitInfo; }

namespace scripting::api
{

struct BonusDescriptor;
struct LuaMetaString;
struct SpellObstacleDescriptor;

class ServerCallbackProxy : public RawPointerWrapper<ServerCallback, ServerCallbackProxy>
{
public:
	using Wrapper = RawPointerWrapper<ServerCallback, ServerCallbackProxy>;

	static const std::vector<typename Wrapper::CustomRegType> REGISTER_CUSTOM;

	static void createUnit(ServerCallback * object, BattleID battleID, battle::UnitInfo info);
	static void updateUnit(ServerCallback * object, BattleID battleID, uint32_t id, JsonNode data, int64_t healthDelta);
	static void removeUnit(ServerCallback * object, BattleID battleID, const battle::Unit * unit);
	static void removeObstacle(ServerCallback * object, BattleID battleID, std::shared_ptr<const CObstacleInstance> obstacle);
	static void moveUnit(ServerCallback * object, BattleID battleID, const battle::Unit * unit, BattleHex destination, bool isTeleport);
	static void appendLog(ServerCallback * object, BattleID battleID, LuaMetaString config);
	static bool describeChanges(ServerCallback * object);
	static void removeUnitBonuses(ServerCallback * object, BattleID battleID, const battle::Unit * unit, const BonusList & bonusList);
	static void addUnitBonus(ServerCallback * object, BattleID battleID, const battle::Unit * unit, BonusDescriptor data, bool cumulative);
	static void addBattleBonus(ServerCallback * object, BattleID battleID, BonusDescriptor data);
	static void addObstacle(ServerCallback * object, BattleID battleID, SpellObstacleDescriptor descriptor);
	static void catapultAttack(ServerCallback * object, BattleID battleID, const battle::Unit * attacker, EWallPart attackedPart, BattleHex destinationTile, int32_t damageDealt, const battle::Unit * killedTowerShooter);
	static int rngInt(lua_State * L); // args: low, high; returns: int in [low, high]
	static int healUnit(lua_State * L);
	static int changeUnit(lua_State * L); // args: battleID, unitState, [healthDelta=0]
	static int damageUnit(lua_State * L); // args: battleID, unit, damageAmount; returns: actualDamage, killedAmount
};

}

VCMI_LIB_NAMESPACE_END
