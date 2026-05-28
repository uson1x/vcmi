/*
 * ServerCallback.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "ServerCallback.h"

#include "../Registry.h"

#include "../../LuaStack.h"
#include "../../../lib/networkPacks/PacksForClientBattle.h"
#include "../../../lib/battle/Unit.h"
#include "../../../lib/battle/CUnitState.h"
#include "../../../lib/json/JsonNode.h"
#include "../../../lib/texts/MetaString.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting
{
namespace api
{

void ServerCallbackProxy::createUnit(ServerCallback * object, BattleID battleID, uint32_t id, JsonNode data)
{
	BattleUnitsChanged buc;
	UnitChanges uc;
	uc.operation = UnitChanges::EOperation::ADD;
	buc.battleID = battleID;
	uc.id = id;
	uc.data = std::move(data);
	buc.changedStacks.push_back(uc);
	object->apply(buc);
}

void ServerCallbackProxy::updateUnit(ServerCallback * object, BattleID battleID, uint32_t id, JsonNode data, int64_t healthDelta)
{
	BattleUnitsChanged buc;
	UnitChanges uc;
	uc.operation = UnitChanges::EOperation::UPDATE;
	buc.battleID = battleID;
	uc.id = id;
	uc.data = std::move(data);
	uc.healthDelta = healthDelta;
	buc.changedStacks.push_back(uc);
	object->apply(buc);
}

void ServerCallbackProxy::injureUnit()
{
}

void ServerCallbackProxy::removeUnit(ServerCallback * object, BattleID battleID, const battle::Unit * unit)
{
	BattleUnitsChanged buc;
	UnitChanges uc;
	buc.battleID = battleID;
	uc.operation = UnitChanges::EOperation::REMOVE;
	uc.id = unit->unitId();
	buc.changedStacks.push_back(uc);
	object->apply(buc);
}

void ServerCallbackProxy::moveUnit(ServerCallback * object, BattleID battleID, const battle::Unit * unit, BattleHex destination, bool isTeleport)
{
	BattleStackMoved pack;
	pack.battleID = battleID;
	pack.stack = unit->unitId();
	pack.distance = 0;
	pack.teleporting = isTeleport;
	BattleHexArray tiles;
	tiles.insert(destination);
	pack.tilesToMove = tiles;
	object->apply(pack);
}

void ServerCallbackProxy::appendLog(ServerCallback * object, BattleID battleID, JsonNode config)
{
	BattleLogMessage msg;
	msg.battleID = battleID;
	msg.lines.push_back(MetaString::createFromLua(config));
	object->apply(msg);
}

const std::vector<ServerCallbackProxy::CustomRegType> ServerCallbackProxy::REGISTER_CUSTOM =
{
	{ "createUnit", LuaFunctionWrapper<&ServerCallbackProxy::createUnit>::invoke, false },
	{ "updateUnit", LuaFunctionWrapper<&ServerCallbackProxy::updateUnit>::invoke, false },
	{ "injureUnit", LuaFunctionWrapper<&ServerCallbackProxy::injureUnit>::invoke, false },
	{ "healUnit",   LuaCallWrapper<&ServerCallbackProxy::healUnit>::invoke,        false },
	{ "removeUnit", LuaFunctionWrapper<&ServerCallbackProxy::removeUnit>::invoke, false },
	{ "moveUnit",   LuaFunctionWrapper<&ServerCallbackProxy::moveUnit>::invoke,   false },
	{ "appendLog",  LuaFunctionWrapper<&ServerCallbackProxy::appendLog>::invoke,  false },
};

int ServerCallbackProxy::healUnit(lua_State * L)
{
	LuaStack S(L);

	ServerCallback * object = nullptr;
	BattleID battleID;
	const battle::Unit * unit = nullptr;
	int64_t healthDelta;
	EHealLevel healLevel;
	EHealPower healPower;

	S.get(1, object);
	S.get(2, battleID);
	S.get(3, unit);
	S.get(4, healthDelta);
	S.get(5, healLevel);
	S.get(6, healPower);

	auto changedUnit = unit->acquire();
	battle::HealInfo info = changedUnit->heal(healthDelta, healLevel, healPower);

	BattleUnitsChanged buc;
	UnitChanges uc;
	buc.battleID = battleID;
	uc.operation = UnitChanges::EOperation::UPDATE;
	uc.id = unit->unitId();
	uc.data = changedUnit->save();
	uc.healthDelta = info.healedHealthPoints;
	buc.changedStacks.push_back(uc);

	object->apply(buc);

	S.clear();
	S.push(info.healedHealthPoints);
	S.push(static_cast<int64_t>(info.resurrectedCount));
	return 2;
}


}
}

VCMI_LIB_NAMESPACE_END
