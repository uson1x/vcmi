/*
 * IBattleInfoCallback.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/Service.h>
#include "../../../lib/battle/IBattleInfoCallback.h"

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

struct CObstacleInstance;

namespace scripting::api
{

class IBattleInfoCallbackProxy : public RawPointerWrapper<const IBattleInfoCallback, IBattleInfoCallbackProxy>
{
public:
	static constexpr std::string_view luaName = "Battle";

	static void registerMethods(MethodRegistrar & R);

	static int getAvailableHex(lua_State * L);
	static int getUnitsIf(lua_State * L);
	static bool isAccessibleForUnit(const IBattleInfoCallback & object, const battle::Unit & unit, BattleHex hex);
	static bool hasPenaltyOnLine(const IBattleInfoCallback & object, BattleHex from, BattleHex dest, bool checkWall, bool checkMoat);
	static const battle::Unit * getUnitByPos(const IBattleInfoCallback & object, BattleHex hex, bool onlyAlive);
	static std::vector<std::shared_ptr<const CObstacleInstance>> getAllObstacles(const IBattleInfoCallback & object);
	static std::vector<std::shared_ptr<const CObstacleInstance>> getObstaclesOnPos(const IBattleInfoCallback & object, BattleHex hex, bool onlyBlocking);
	static bool hasFortifications(const IBattleInfoCallback & object);
	static bool hasMoat(const IBattleInfoCallback & object);
	static bool hasNativeStack(const IBattleInfoCallback & object, BattleSide side);
	static BattleHexArray getAllPossibleHexes(const IBattleInfoCallback & object);
	static std::optional<EWallState> getWallState(const IBattleInfoCallback & object, EWallPart part);
	static bool isWallPartAttackable(const IBattleInfoCallback & object, EWallPart part);
	static BattleHex wallPartToBattleHex(const IBattleInfoCallback & object, EWallPart part);
	static EWallPart hexToWallPart(const IBattleInfoCallback & object, BattleHex hex);
	static BattleHex getTowerShooterHex(const IBattleInfoCallback & object, EWallPart part);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<IBattleInfoCallback>)
{
	return std::string(IBattleInfoCallbackProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
