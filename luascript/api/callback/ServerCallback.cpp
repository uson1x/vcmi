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

#include "GameLibrary.h"
#include "ServerCallback.h"

#include "../LuaMetaString.h"
#include "../Registry.h"
#include "../battle/UnitState.h"
#include "../battle/SpellObstacleDescriptor.h"
#include "../library/BonusDescriptor.h"

#include "../../LuaStack.h"
#include "../../../lib/networkPacks/PacksForClientBattle.h"
#include "../../../lib/networkPacks/SetStackEffect.h"
#include "../../../lib/battle/Unit.h"
#include "../../../lib/battle/CObstacleInstance.h"
#include "../../../lib/CStack.h"
#include "../../../lib/bonuses/BonusList.h"
#include "../../../lib/bonuses/Bonus.h"
#include "../../../lib/battle/CUnitState.h"
#include "../../../lib/texts/MetaString.h"
#include "../../../lib/constants/EntityIdentifiers.h"
#include "modding/IdentifierStorage.h"
#include "modding/ModScope.h"

#include <vstd/RNG.h>

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

const std::vector<ServerCallbackProxy::CustomRegType> ServerCallbackProxy::REGISTER_CUSTOM =
{
	{ "createUnit",        LuaFunctionWrapper<&ServerCallbackProxy::createUnit>::invoke,        false },
	{ "healUnit",          LuaCallWrapper<&ServerCallbackProxy::healUnit>::invoke,              false },
	{ "changeUnit",        LuaCallWrapper<&ServerCallbackProxy::changeUnit>::invoke,            false },
	{ "damageUnit",        LuaCallWrapper<&ServerCallbackProxy::damageUnit>::invoke,            false },
	{ "removeUnit",        LuaFunctionWrapper<&ServerCallbackProxy::removeUnit>::invoke,        false },
	{ "removeObstacle",    LuaFunctionWrapper<&ServerCallbackProxy::removeObstacle>::invoke,    false },
	{ "moveUnit",          LuaFunctionWrapper<&ServerCallbackProxy::moveUnit>::invoke,          false },
	{ "appendLog",         LuaFunctionWrapper<&ServerCallbackProxy::appendLog>::invoke,         false },
	{ "describeChanges",   LuaFunctionWrapper<&ServerCallbackProxy::describeChanges>::invoke,   false },
	{ "removeUnitBonuses", LuaFunctionWrapper<&ServerCallbackProxy::removeUnitBonuses>::invoke, false },
	{ "addUnitBonus",      LuaFunctionWrapper<&ServerCallbackProxy::addUnitBonus>::invoke,      false },
	{ "addBattleBonus",    LuaFunctionWrapper<&ServerCallbackProxy::addBattleBonus>::invoke,    false },
	{ "addObstacle",       LuaFunctionWrapper<&ServerCallbackProxy::addObstacle>::invoke,       false },
	{ "catapultAttack",    LuaFunctionWrapper<&ServerCallbackProxy::catapultAttack>::invoke,    false },
	{ "rngInt",            LuaCallWrapper<&ServerCallbackProxy::rngInt>::invoke,                false },
};

bool ServerCallbackProxy::describeChanges(ServerCallback * object)
{
	return object->describeChanges();
}

void ServerCallbackProxy::removeUnitBonuses(ServerCallback * object, BattleID battleID, const battle::Unit * unit, const BonusList & bonusList)
{
	std::vector<Bonus> buffer;
	for(const auto & b : bonusList)
		buffer.emplace_back(*b);

	if(buffer.empty())
		return;

	SetStackEffect sse;
	sse.battleID = battleID;
	sse.toRemove.emplace_back(unit->unitId(), buffer);
	object->apply(sse);
}

void ServerCallbackProxy::addUnitBonus(ServerCallback * object, BattleID battleID, const battle::Unit * unit, BonusDescriptor data, bool cumulative)
{
	Bonus b = data.toBonus();

	SetStackEffect sse;
	sse.battleID = battleID;
	if(cumulative)
		sse.toAdd.emplace_back(unit->unitId(), std::vector<Bonus>{b});
	else
		sse.toUpdate.emplace_back(unit->unitId(), std::vector<Bonus>{b});
	object->apply(sse);
}

void ServerCallbackProxy::addBattleBonus(ServerCallback * object, BattleID battleID, BonusDescriptor data)
{
	GiveBonus gb(GiveBonus::ETarget::BATTLE);
	gb.id = battleID;
	gb.bonus = data.toBonus();
	object->apply(gb);
}

void ServerCallbackProxy::addObstacle(ServerCallback * object, BattleID battleID, SpellObstacleDescriptor descriptor)
{
	SpellCreatedObstacle obstacle = descriptor.toObstacle();

	BattleObstaclesChanged pack;
	pack.battleID = battleID;
	pack.changes.emplace_back();
	obstacle.toInfo(pack.changes.back());
	object->apply(pack);
}


void ServerCallbackProxy::createUnit(ServerCallback * object, BattleID battleID, battle::UnitInfo info)
{
	BattleUnitsChanged buc;
	UnitChanges uc;
	uc.operation = UnitChanges::EOperation::ADD;
	buc.battleID = battleID;
	uc.id = info.id;
	info.save(uc.data);
	buc.changedStacks.push_back(uc);
	object->apply(buc);
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

void ServerCallbackProxy::removeObstacle(ServerCallback * object, BattleID battleID, std::shared_ptr<const CObstacleInstance> obstacle)
{
	if(!obstacle)
		return;

	BattleObstaclesChanged pack;
	pack.battleID = battleID;
	pack.changes.emplace_back(obstacle->uniqueID, BattleChanges::EOperation::REMOVE);
	auto * serializable = const_cast<CObstacleInstance*>(obstacle.get());
	serializable->toInfo(pack.changes.back(), BattleChanges::EOperation::REMOVE);
	object->apply(pack);
}

void ServerCallbackProxy::catapultAttack(ServerCallback * object, BattleID battleID, const battle::Unit * attacker, EWallPart attackedPart, BattleHex destinationTile, int32_t damageDealt, const battle::Unit * killedTowerShooter)
{
	CatapultAttack ca;
	ca.battleID = battleID;
	ca.attacker = attacker ? attacker->unitId() : -1;
	ca.attackedPart = attackedPart;
	ca.destinationTile = destinationTile.toInt();
	ca.damageDealt = static_cast<ui8>(std::clamp(damageDealt, 0, 255));
	ca.killedTowerShooter = killedTowerShooter ? killedTowerShooter->unitId() : -1;
	object->apply(ca);
}

int ServerCallbackProxy::rngInt(lua_State * L)
{
	LuaStack S(L);

	ServerCallback * object = nullptr;
	int low = 0;
	int high = 0;

	S.getNonNull(1, object);
	S.get(2, low);
	S.get(3, high);

	int result = object->getRNG()->nextInt(low, high);

	S.clear();
	S.push(result);
	return 1;
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

void ServerCallbackProxy::appendLog(ServerCallback * object, BattleID battleID, LuaMetaString config)
{
	BattleLogMessage msg;
	msg.battleID = battleID;
	msg.lines.push_back(config.toMetaString());
	object->apply(msg);
}

int ServerCallbackProxy::changeUnit(lua_State * L)
{
	LuaStack S(L);

	ServerCallback * object = nullptr;
	BattleID battleID;
	LuaUnitState unitState;
	int64_t healthDelta = 0;

	S.get(1, object);
	S.get(2, battleID);
	S.get(3, unitState);
	if(S.stackSize() >= 4)
		S.get(4, healthDelta);

	auto * cstate = unitState.getState();

	BattleUnitsChanged buc;
	UnitChanges uc(cstate->unitId(), UnitChanges::EOperation::UPDATE);
	buc.battleID = battleID;
	uc.data = cstate->save();
	uc.healthDelta = healthDelta;
	buc.changedStacks.push_back(uc);
	object->apply(buc);

	return S.retVoid();
}

int ServerCallbackProxy::damageUnit(lua_State * L)
{
	LuaStack S(L);

	ServerCallback * object = nullptr;
	BattleID battleID;
	const battle::Unit * unit = nullptr;
	int64_t damageAmount = 0;

	S.get(1, object);
	S.get(2, battleID);
	S.getNonNull(3, unit);
	S.get(4, damageAmount);

	BattleStackAttacked bsa;
	bsa.damageAmount = damageAmount;
	bsa.stackAttacked = unit->unitId();
	bsa.attackerID = -1;
	auto newState = unit->acquireState();
	CStack::prepareAttacked(bsa, *object->getRNG(), newState);

	StacksInjured si;
	si.battleID = battleID;
	si.stacks.push_back(bsa);
	object->apply(si);

	S.clear();
	S.push(bsa.damageAmount);
	S.push(static_cast<int64_t>(bsa.killedAmount));
	return 2;
}

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
	S.getNonNull(3, unit);
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

VCMI_LIB_NAMESPACE_END
