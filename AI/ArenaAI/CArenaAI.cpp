#include "StdInc.h"
#include "CArenaAI.h"

#include "../../lib/callback/CCallback.h"
#include "../../lib/ConditionalWait.h"
#include "../../lib/battle/BattleAction.h"
#include "../../lib/constants/Enumerations.h"
#include "../../lib/GameConstants.h"
#include "../../lib/IGameSettings.h"
#include "../../lib/UnlockGuard.h"
#include "../../lib/entities/building/CBuilding.h"
#include "../../lib/entities/hero/CHero.h"
#include "../../lib/gameState/CGameState.h"
#include "../../lib/json/JsonNode.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGObjectInstance.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/mapObjects/army/CArmedInstance.h"
#include "../../lib/mapObjects/army/CCreatureSet.h"
#include "../../lib/mapping/TerrainTile.h"
#include "../../lib/networkPacks/Component.h"
#include "../../lib/pathfinder/CGPathNode.h"
#include "../../lib/pathfinder/PathfinderOptions.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <limits>
#include <set>
#include <shared_mutex>
#include <tuple>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
const char * BRIDGE_SCHEMA_VERSION = "vcmi_arena_bridge.v1";
constexpr int MAX_FRIENDLY_HEROES = 12;
constexpr int MAX_FRIENDLY_TOWNS = 12;
constexpr int MAX_ENEMY_HEROES = 12;
constexpr int MAX_ENEMY_TOWNS = 12;
constexpr int MAX_VISIBLE_OBJECTS = 48;
constexpr int MAX_MOVE_OPTIONS = 24;
constexpr int MAX_BUILD_OPTIONS = 36;
constexpr int MAX_RECRUIT_OPTIONS = 36;
constexpr int MAX_RECRUIT_HERO_OPTIONS = 8;
constexpr int MAX_MANAGE_ARMY_OPTIONS = 16;

// "Gate-class" objects gate the map's topology (routes between quadrants/levels and the
// win-condition targets): the pathfinding bottlenecks a policy MUST see to plan multi-turn
// travel. On dense 72x72 maps the visible-object list far exceeds MAX_VISIBLE_OBJECTS, so a
// blind cap can permanently hide a keymaster tent or subterranean gate even while its MOVE
// offer (uncapped) exists. These are force-included ahead of ordinary objects under the cap,
// and the remaining slots fill nearest-first (see the visibleObjects ordering below).
bool isGateClassObject(const CGObjectInstance * obj)
{
	if(obj == nullptr)
		return false;
	switch(obj->ID.toEnum())
	{
	case Obj::BORDERGUARD:
	case Obj::KEYMASTER:
	case Obj::BORDER_GATE:
	case Obj::QUEST_GUARD:
	case Obj::SUBTERRANEAN_GATE:
	case Obj::MONOLITH_ONE_WAY_ENTRANCE:
	case Obj::MONOLITH_ONE_WAY_EXIT:
	case Obj::MONOLITH_TWO_WAY:
	case Obj::WHIRLPOOL:
	case Obj::SHIPYARD:
	case Obj::GARRISON:
	case Obj::GARRISON2:
	case Obj::TOWN:
		return true;
	default:
		return false;
	}
}

std::string sideFromPlayer(const PlayerColor & player)
{
	if(player.getNum() == 0)
		return "red";
	if(player.getNum() == 1)
		return "blue";
	return (player.getNum() % 2 == 0) ? "red" : "blue";
}

std::string turnIdFor(const std::string & matchId, const PlayerColor & player, int day)
{
	return matchId + "-p" + std::to_string(player.getNum()) + "-d" + std::to_string(day);
}

std::string nowUtcSeconds()
{
	return std::to_string(static_cast<long long>(std::time(nullptr)));
}

int clampTimeoutMs(int timeout)
{
	if(timeout < 100)
		return 100;
	if(timeout > 120000)
		return 120000;
	return timeout;
}

int clampActionsPerTurn(int count)
{
	if(count < 1)
		return 1;
	if(count > 64)
		return 64;
	return count;
}

int clampTurnBudgetMs(int timeout)
{
	if(timeout < 1000)
		return 1000;
	if(timeout > 900000)
		return 900000;
	return timeout;
}

std::string ownerRelation(const PlayerColor & owner, const PlayerColor & perspective)
{
	if(owner == perspective)
		return "friendly";
	if(owner.isValidPlayer())
		return "enemy";
	return "neutral";
}

// The contract's absolute owner naming: player 0 (red) is the attacker seat, player 1
// (blue) the defender, matching the Python bridge's side aliases. Unowned/uncolored
// objects are "neutral". Unlike `relation` this does not depend on whose turn it is,
// so bots can compare it against acting_side directly.
std::string ownerBridgeSide(const PlayerColor & owner)
{
	if(!owner.isValidPlayer())
		return "neutral";
	if(owner.getNum() == 0)
		return "attacker";
	if(owner.getNum() == 1)
		return "defender";
	return (owner.getNum() % 2 == 0) ? "attacker" : "defender";
}

void pushResourceSnapshot(JsonNode & resourcesNode, const ResourceSet & resources)
{
	resourcesNode["wood"].Integer() = resources[EGameResID::WOOD];
	resourcesNode["mercury"].Integer() = resources[EGameResID::MERCURY];
	resourcesNode["ore"].Integer() = resources[EGameResID::ORE];
	resourcesNode["sulfur"].Integer() = resources[EGameResID::SULFUR];
	resourcesNode["crystal"].Integer() = resources[EGameResID::CRYSTAL];
	resourcesNode["gems"].Integer() = resources[EGameResID::GEMS];
	resourcesNode["gold"].Integer() = resources[EGameResID::GOLD];
}

// Emit only the NON-ZERO resource amounts of a building cost, the way the build
// screen shows a price (e.g. {gold:1000, ore:5}). Lets the model see WHAT a
// building costs and (combined with its resource bar) WHICH resource gates it.
void pushBuildingCost(JsonNode & costNode, const ResourceSet & cost)
{
	static const std::pair<EGameResID, const char *> kinds[] = {
		{EGameResID::WOOD, "wood"}, {EGameResID::MERCURY, "mercury"}, {EGameResID::ORE, "ore"},
		{EGameResID::SULFUR, "sulfur"}, {EGameResID::CRYSTAL, "crystal"}, {EGameResID::GEMS, "gems"},
		{EGameResID::GOLD, "gold"},
	};
	for(const auto & kind : kinds)
	{
		const int amount = cost[kind.first];
		if(amount != 0)
			costNode[kind.second].Integer() = amount;
	}
}

// Render an army's stacks (hero or town garrison) so the model can reason about
// specific creatures/counts instead of a single opaque army_power scalar. This is
// the core of T2's "fight smart" visibility: knowing you have 5 weak stacks vs one
// strong one changes whether you should press into a guarded fight.
void pushArmyStacks(JsonNode & stacksNode, const CCreatureSet & army)
{
	stacksNode.setType(JsonNode::JsonType::DATA_VECTOR);
	for(const auto & slot : army.Slots())
	{
		const SlotID slotId = slot.first;
		const CCreature * creature = army.getCreature(slotId);
		if(creature == nullptr)
			continue;
		JsonNode node;
		node["slot"].Integer() = slotId.getNum();
		node["unit"].String() = creature->getJsonKey();
		node["name"].String() = creature->getNameSingularTranslated();
		node["count"].Integer() = army.getStackCount(slotId);
		node["power"].Integer() = static_cast<si64>(army.getPower(slotId));
		node["level"].Integer() = static_cast<si64>(creature->getLevel());
		stacksNode.Vector().push_back(node);
	}
}

std::string heroToken(int heroId)
{
	return "h_" + std::to_string(heroId);
}

std::string townToken(int townId)
{
	return "t_" + std::to_string(townId);
}

std::string objectToken(int objectId)
{
	return "o_" + std::to_string(objectId);
}

} // namespace

void CArenaAI::initGameInterface(std::shared_ptr<Environment> env_, std::shared_ptr<CCallback> callback)
{
	cb = callback;
	env = env_;
	cbc = callback;
	human = false;
	playerID = *cb->getPlayerID();
	sideName = sideFromPlayer(playerID);

	if(const char * value = std::getenv("ARENA_MATCH_ID"))
	{
		if(std::strlen(value) > 0)
			matchId = value;
	}
	if(const char * value = std::getenv("ARENA_BRIDGE_SOCKET"))
	{
		socketPath = value;
	}
	if(const char * value = std::getenv("ARENA_DECISION_TIMEOUT_MS"))
	{
		try
		{
			decisionTimeoutMs = clampTimeoutMs(std::stoi(value));
		}
		catch(...)
		{
			decisionTimeoutMs = 8000;
		}
	}
	turnTimeBudgetMs = clampTurnBudgetMs(decisionTimeoutMs * maxActionsPerTurn);
	if(const char * value = std::getenv("ARENA_MAX_ACTIONS_PER_TURN"))
	{
		try
		{
			maxActionsPerTurn = clampActionsPerTurn(std::stoi(value));
		}
		catch(...)
		{
			maxActionsPerTurn = 8;
		}
		turnTimeBudgetMs = clampTurnBudgetMs(decisionTimeoutMs * maxActionsPerTurn);
	}
	if(const char * value = std::getenv("ARENA_TURN_TIMEOUT_MS"))
	{
		try
		{
			turnTimeBudgetMs = clampTurnBudgetMs(std::stoi(value));
		}
		catch(...)
		{
			turnTimeBudgetMs = clampTurnBudgetMs(decisionTimeoutMs * maxActionsPerTurn);
		}
	}
	if(const char * value = std::getenv("ARENA_BATTLE_AI"))
	{
		if(std::strlen(value) > 0)
			battleAiName = value;
	}

	bridgeEnabled = !socketPath.empty();

	// Block adventure requests until the server has realized them. Combined with
	// running the turn loop off the network thread (see yourTurn/runTurn), a move
	// that starts a battle blocks until the battle auto-resolves via the inherited
	// CAdventureAI -> BattleAI delegation, instead of deadlocking on the CBattleQuery.
	cb->waitTillRealize = true;
}

CArenaAI::~CArenaAI()
{
	aborting = true;
	battleCv.notify_all();
	if(turnThread.joinable())
		turnThread.join();
}

std::string CArenaAI::getBattleAIName() const
{
	return battleAiName;
}

JsonNode CArenaAI::buildTurnRequestPayload(QueryID queryID, int actionIndex, int turnRemainingMs) const
{
	JsonNode payload;
	const int day = cb->getCalendar().getCurrentDay();
	const std::string baseTurnId = turnIdFor(matchId, playerID, day);
	auto heroes = cb->getHeroesInfo();
	auto towns = cb->getTownsInfo(true);
	auto visibleTowns = cb->getTownsInfo(false);
	auto visibleObjects = cb->getAllVisitableObjs();

	std::sort(heroes.begin(), heroes.end(), [](const CGHeroInstance * left, const CGHeroInstance * right)
	{
		if(left == nullptr || right == nullptr)
			return left != nullptr;
		return left->id.getNum() < right->id.getNum();
	});

	std::sort(towns.begin(), towns.end(), [](const CGTownInstance * left, const CGTownInstance * right)
	{
		if(left == nullptr || right == nullptr)
			return left != nullptr;
		return left->id.getNum() < right->id.getNum();
	});

	std::sort(visibleTowns.begin(), visibleTowns.end(), [](const CGTownInstance * left, const CGTownInstance * right)
	{
		if(left == nullptr || right == nullptr)
			return left != nullptr;
		return left->id.getNum() < right->id.getNum();
	});

	// Object emission ordering (fix 7): the old sort was purely by object id (map creation
	// order), so on dense maps the MAX_VISIBLE_OBJECTS cap dropped whatever happened to have a
	// high id — often the very tents/gates a policy needs to plan cross-map travel, hidden
	// PERMANENTLY. Now: gate-class objects first (topology bottlenecks + win-condition
	// targets), then everything else NEAREST-FIRST to the acting player's closest hero. Ties
	// break by object id so the ordering stays deterministic for ranked replay. The cap then
	// keeps the actionable near field plus all the gates instead of an id-arbitrary slice.
	std::vector<int3> heroPositions;
	for(const auto * h : heroes)
		if(h != nullptr)
			heroPositions.push_back(h->visitablePos());
	auto distSqToNearestHero = [&heroPositions](const CGObjectInstance * obj) -> long long
	{
		if(obj == nullptr || heroPositions.empty())
			return std::numeric_limits<long long>::max();
		const int3 p = obj->visitablePos();
		long long best = std::numeric_limits<long long>::max();
		for(const int3 & hp : heroPositions)
		{
			// same-level distance only; cross-level objects rank last within their class,
			// which is correct — you cannot walk to them without first taking a gate.
			if(hp.z != p.z)
				continue;
			const long long dx = hp.x - p.x;
			const long long dy = hp.y - p.y;
			best = std::min(best, dx * dx + dy * dy);
		}
		return best;
	};
	std::sort(visibleObjects.begin(), visibleObjects.end(),
		[&distSqToNearestHero](const CGObjectInstance * left, const CGObjectInstance * right)
	{
		if(left == nullptr || right == nullptr)
			return left != nullptr;
		const bool lg = isGateClassObject(left);
		const bool rg = isGateClassObject(right);
		if(lg != rg)
			return lg; // gate-class objects sort ahead of ordinary ones
		const long long ld = distSqToNearestHero(left);
		const long long rd = distSqToNearestHero(right);
		if(ld != rd)
			return ld < rd; // then nearest-first to the acting player's closest hero
		return left->id.getNum() < right->id.getNum();
	});

	payload["turn_id"].String() = baseTurnId + "-a" + std::to_string(actionIndex);
	payload["turn_base_id"].String() = baseTurnId;
	payload["turn_index"].Integer() = day;
	payload["day_index"].Integer() = day;
	payload["day"].Integer() = day;
	payload["turn_action_index"].Integer() = actionIndex;
	payload["acting_side"].String() = sideName;
	payload["query_id"].Integer() = queryID.getNum();
	const auto mapSize = cb->getMapSize();
	payload["visible_state"]["map"]["width"].Integer() = mapSize.x;
	payload["visible_state"]["map"]["height"].Integer() = mapSize.y;
	payload["visible_state"]["map"]["levels"].Integer() = mapSize.z;

	payload["visible_state"]["day"].Integer() = day;
	payload["visible_state"]["acting_side"].String() = sideName;
	payload["visible_state"]["friendly"]["hero_count"].Integer() = static_cast<si64>(heroes.size());
	payload["visible_state"]["friendly"]["town_count"].Integer() = static_cast<si64>(towns.size());
	payload["visible_state"]["friendly"]["resources_known"].Bool() = true;
	pushResourceSnapshot(payload["visible_state"]["friendly"]["resources"], cb->getResourceAmount());

	JsonNode friendlyHeroNodes;
	friendlyHeroNodes.setType(JsonNode::JsonType::DATA_VECTOR);
	for(size_t idx = 0; idx < heroes.size() && idx < static_cast<size_t>(MAX_FRIENDLY_HEROES); ++idx)
	{
		const auto * hero = heroes[idx];
		if(hero == nullptr)
			continue;
		const int3 pos = hero->visitablePos();
		JsonNode heroNode;
		heroNode["id"].String() = heroToken(hero->id.getNum());
		heroNode["name"].String() = hero->getNameTranslated();
		heroNode["side"].String() = sideName;
		heroNode["x"].Integer() = pos.x;
		heroNode["y"].Integer() = pos.y;
		heroNode["z"].Integer() = pos.z;
		heroNode["movement"].Integer() = hero->movementPointsRemaining();
		heroNode["army_power"].Integer() = static_cast<si64>(hero->getTotalStrength());
		heroNode["level"].Integer() = hero->level;
		heroNode["mana"].Integer() = hero->mana;
		JsonNode heroArmyNodes;
		pushArmyStacks(heroArmyNodes, *hero);
		heroNode["army"] = heroArmyNodes;
		friendlyHeroNodes.Vector().push_back(heroNode);
	}
	payload["visible_state"]["friendly"]["heroes"] = friendlyHeroNodes;

	JsonNode friendlyTownNodes;
	friendlyTownNodes.setType(JsonNode::JsonType::DATA_VECTOR);
	for(size_t idx = 0; idx < towns.size() && idx < static_cast<size_t>(MAX_FRIENDLY_TOWNS); ++idx)
	{
		const auto * town = towns[idx];
		if(town == nullptr)
			continue;

		const int3 pos = town->visitablePos();
		JsonNode townNode;
		townNode["id"].String() = townToken(town->id.getNum());
		townNode["name"].String() = town->getNameTranslated();
		townNode["side"].String() = sideName;
		townNode["x"].Integer() = pos.x;
		townNode["y"].Integer() = pos.y;
		townNode["z"].Integer() = pos.z;
		townNode["army_power"].Integer() = static_cast<si64>(town->getArmyStrength());

		JsonNode buildingNodes;
		buildingNodes.setType(JsonNode::JsonType::DATA_VECTOR);
		for(const auto & built : town->getBuildings())
		{
			const auto it = town->getTown()->buildings.find(built);
			if(it == town->getTown()->buildings.end() || !it->second)
				continue;
			JsonNode buildingNode;
			buildingNode.String() = it->second->getJsonKey();
			buildingNodes.Vector().push_back(buildingNode);
		}
		townNode["buildings"] = buildingNodes;

		// Build-screen view: buildings NOT yet built that are currently LOCKED — gated by
		// missing RESOURCES or PREREQUISITES. A human reads this off the greyed-out build
		// screen (each building shows its price and is greyed if unaffordable); without it
		// the model cannot see WHICH resource gates its army (e.g. creature dwellings need
		// ore), so it can never learn to go secure that resource. Each entry carries the
		// full cost; the model compares it to its resource bar to see the shortfall.
		JsonNode buildLockedNodes;
		buildLockedNodes.setType(JsonNode::JsonType::DATA_VECTOR);
		int lockedEmitted = 0;
		for(const auto & buildingEntry : town->getTown()->buildings)
		{
			if(lockedEmitted >= MAX_BUILD_OPTIONS)
				break;
			const auto * building = buildingEntry.second.get();
			if(building == nullptr)
				continue;
			if(town->hasBuilt(buildingEntry.first))
				continue;
			const auto state = cb->canBuildStructure(town, buildingEntry.first);
			const char * stateStr = nullptr;
			if(state == EBuildingState::NO_RESOURCES)
				stateStr = "need_resources";
			else if(state == EBuildingState::PREREQUIRES || state == EBuildingState::MISSING_BASE)
				stateStr = "need_prereq";
			else
				continue; // ALLOWED is a legal action; skip FORBIDDEN/ALREADY_PRESENT/CANT_BUILD_TODAY/etc.

			JsonNode node;
			node["building"].String() = building->getJsonKey();
			node["building_name"].String() = building->getNameTranslated();
			node["state"].String() = stateStr;
			JsonNode costNode;
			pushBuildingCost(costNode, building->resources);
			node["cost"] = costNode;
			buildLockedNodes.Vector().push_back(node);
			++lockedEmitted;
		}
		townNode["build_locked"] = buildLockedNodes;

		// Optional diagnostic (env ARENA_DEBUG_BUILD_STATES=1): raw EBuildingState for every
		// not-built building. Used to verify map facts like "dwellingLvl2-7 are FORBIDDEN on
		// this map" — the skill requires proving structural claims, not inferring them. Off in
		// production (no env), so it never reaches the model.
		if(std::getenv("ARENA_DEBUG_BUILD_STATES") != nullptr)
		{
			JsonNode dbg;
			dbg.setType(JsonNode::JsonType::DATA_VECTOR);
			for(const auto & be : town->getTown()->buildings)
			{
				if(be.second.get() == nullptr || town->hasBuilt(be.first))
					continue;
				JsonNode n;
				n["building"].String() = be.second->getJsonKey();
				n["state_int"].Integer() = static_cast<int>(cb->canBuildStructure(town, be.first));
				dbg.Vector().push_back(n);
			}
			townNode["build_all_states_DEBUG"] = dbg;
		}

		JsonNode recruitNodes;
		recruitNodes.setType(JsonNode::JsonType::DATA_VECTOR);
		for(size_t level = 0; level < town->creatures.size(); ++level)
		{
			const auto & entry = town->creatures[level];
			if(entry.first <= 0 || entry.second.empty())
				continue;
			const auto creatureId = entry.second.front();
			const auto * creature = creatureId.toCreature();
			if(creature == nullptr)
				continue;
			JsonNode recruitNode;
			recruitNode["unit"].String() = creature->getJsonKey();
			recruitNode["name"].String() = creature->getNameSingularTranslated();
			recruitNode["available"].Integer() = static_cast<si64>(entry.first);
			recruitNode["level"].Integer() = static_cast<si64>(level + 1);
			recruitNodes.Vector().push_back(recruitNode);
		}
		townNode["recruit_pool"] = recruitNodes;

		JsonNode garrisonNodes;
		pushArmyStacks(garrisonNodes, *town);
		townNode["garrison"] = garrisonNodes;
		friendlyTownNodes.Vector().push_back(townNode);
	}
	payload["visible_state"]["friendly"]["towns"] = friendlyTownNodes;

	JsonNode enemyHeroNodes;
	enemyHeroNodes.setType(JsonNode::JsonType::DATA_VECTOR);
	std::set<int> pushedEnemyHeroes;
	for(const auto * object : visibleObjects)
	{
		const auto * hero = dynamic_cast<const CGHeroInstance *>(object);
		if(hero == nullptr)
			continue;
		if(hero->tempOwner == playerID || !hero->tempOwner.isValidPlayer())
			continue;
		if(pushedEnemyHeroes.count(hero->id.getNum()) != 0)
			continue;
		if(enemyHeroNodes.Vector().size() >= static_cast<size_t>(MAX_ENEMY_HEROES))
			break;

		const int3 pos = hero->visitablePos();
		JsonNode heroNode;
		heroNode["id"].String() = heroToken(hero->id.getNum());
		heroNode["name"].String() = hero->getNameTranslated();
		heroNode["side"].String() = sideFromPlayer(hero->tempOwner);
		heroNode["x"].Integer() = pos.x;
		heroNode["y"].Integer() = pos.y;
		heroNode["z"].Integer() = pos.z;
		heroNode["movement"].Integer() = hero->movementPointsRemaining();
		heroNode["army_power"].Integer() = static_cast<si64>(hero->getTotalStrength());
		enemyHeroNodes.Vector().push_back(heroNode);
		pushedEnemyHeroes.insert(hero->id.getNum());
	}
	payload["visible_state"]["enemy_visible"]["heroes"] = enemyHeroNodes;

	JsonNode enemyTownNodes;
	enemyTownNodes.setType(JsonNode::JsonType::DATA_VECTOR);
	for(size_t idx = 0; idx < visibleTowns.size() && enemyTownNodes.Vector().size() < static_cast<size_t>(MAX_ENEMY_TOWNS); ++idx)
	{
		const auto * town = visibleTowns[idx];
		if(town == nullptr || town->tempOwner == playerID)
			continue;
		const int3 pos = town->visitablePos();
		JsonNode townNode;
		townNode["id"].String() = townToken(town->id.getNum());
		townNode["name"].String() = town->getNameTranslated();
		townNode["side"].String() = sideFromPlayer(town->tempOwner);
		townNode["x"].Integer() = pos.x;
		townNode["y"].Integer() = pos.y;
		townNode["z"].Integer() = pos.z;
		townNode["army_power"].Integer() = static_cast<si64>(town->getArmyStrength());
		enemyTownNodes.Vector().push_back(townNode);
	}
	payload["visible_state"]["enemy_visible"]["towns"] = enemyTownNodes;

	JsonNode objectNodes;
	objectNodes.setType(JsonNode::JsonType::DATA_VECTOR);
	for(size_t idx = 0; idx < visibleObjects.size() && idx < static_cast<size_t>(MAX_VISIBLE_OBJECTS); ++idx)
	{
		const auto * object = visibleObjects[idx];
		if(object == nullptr)
			continue;
		const int3 pos = object->visitablePos();
		JsonNode node;
		node["id"].String() = objectToken(object->id.getNum());
		node["type"].String() = object->getTypeName();
		node["name"].String() = object->getObjectName();
		node["x"].Integer() = pos.x;
		node["y"].Integer() = pos.y;
		node["z"].Integer() = pos.z;
		node["relation"].String() = ownerRelation(object->tempOwner, playerID);
		node["owner"].String() = ownerBridgeSide(object->tempOwner);
		if(object->tempOwner.isValidPlayer())
			node["owner_side"].String() = sideFromPlayer(object->tempOwner);
		objectNodes.Vector().push_back(node);
	}
	payload["visible_state"]["map_objects_visible"] = objectNodes;

	payload["unknown_state"]["fog_of_war_enabled"].Bool() = true;
	payload["unknown_state"]["enemy_resources"].String() = "unknown";
	payload["unknown_state"]["hidden_map_objects"].String() = "unknown";
	payload["unknown_state"]["hidden_enemy_heroes"].String() = "unknown";

	JsonNode legalActions;
	legalActions.setType(JsonNode::JsonType::DATA_VECTOR);

	JsonNode endTurn;
	endTurn["type"].String() = "END_TURN";
	legalActions.Vector().push_back(endTurn);

	JsonNode moveGroup;
	moveGroup["type"].String() = "MOVE_HERO";
	JsonNode moveOptions;
	moveOptions.setType(JsonNode::JsonType::DATA_VECTOR);

	// Assign a direction octant (0..7) to a (dx,dy) offset from the hero.
	auto octantOf = [](int dx, int dy) -> int {
		if(dx == 0 && dy == 0)
			return -1;
		const int adx = std::abs(dx);
		const int ady = std::abs(dy);
		if(adx >= 2 * ady)
			return dx > 0 ? 0 : 4; // E / W
		if(ady >= 2 * adx)
			return dy > 0 ? 2 : 6; // S / N
		if(dx > 0)
			return dy > 0 ? 1 : 7; // SE / NE
		return dy > 0 ? 3 : 5;     // SW / NW
	};

	// T2 threat estimate: total army strength of the wandering monster(s) guarding a
	// destination tile. Surfaced on the move option so the model can compare it to its
	// own army_power and refuse fights it can only win by bleeding out (the T1 failure
	// mode: the hero won guard battles for territory but lost its army doing it).
	auto guardPowerAt = [this](const int3 & pos, std::string & nameOut, long long & countOut) -> long long {
		long long total = 0;
		countOut = 0;
		std::set<int> countedObjects;
		auto addArmed = [&](const CGObjectInstance * obj)
		{
			const auto * armed = dynamic_cast<const CArmedInstance *>(obj);
			if(armed == nullptr)
				return;
			if(!countedObjects.insert(obj->id.getNum()).second)
				return; // same defender reachable twice (ZoC + on-tile) — count once
			total += static_cast<long long>(armed->getArmyStrength());
			// Total creature head-count across the guard's stacks. The harness renders this as
			// a fuzzy HoMM3 quantity descriptor ("a pack of Wolves") — what a real player sees,
			// instead of the precise AI-value (which is not shown in-game).
			for(const auto & slotPair : armed->Slots())
				countOut += static_cast<long long>(armed->getStackCount(slotPair.first));
			if(nameOut.empty())
				nameOut = obj->getObjectName();
		};
		// Wandering monsters whose zone of control covers the tile (on it or adjacent).
		for(const auto * guard : cb->getGuardingCreatures(pos))
			addArmed(guard);
		// Armed DEFENDERS sitting on the tile itself: garrison objects, town garrisons
		// (plus a visiting hero, a separate visitable at the same tile), enemy heroes,
		// creature banks. getGuardingCreatures covers Obj::MONSTER only, so these were
		// previously invisible — the offer carried null guard fields and heroes walked
		// blind into e.g. a quadrant-gating garrison holding a 4x stronger army.
		for(const auto * obj : cb->getVisitableObjs(pos, false))
		{
			if(obj == nullptr || obj->ID == Obj::MONSTER)
				continue; // on-tile monsters are already in the ZoC list
			if(obj->tempOwner == playerID)
				continue; // own objects do not fight us
			addArmed(obj);
		}
		return total;
	};

	int emitted = 0;
	for(const auto * hero : heroes)
	{
		if(hero == nullptr)
			continue;
		const int3 from = hero->visitablePos();

		// T0: drive movement options off the engine's own pathfinder rather than the
		// 8 immediate neighbours. The old terrain-only adjacent check boxed heroes in
		// (the only adjacent tiles were often blocking objects) and advertised moves the
		// server then refused as "...request must have been fishy!". Instead we scan the
		// reachable-this-turn, standable (NORMAL) tiles and offer the FARTHEST one per
		// compass direction, so a single decision makes real map progress and every
		// offered destination is one the engine will actually accept.
		CPathsInfo heroPaths(cb->getMapSize(), hero);
		auto pathConfig = std::make_shared<SingleHeroPathfinderConfig>(heroPaths, *cb, hero);
		cb->calculatePaths(pathConfig);

		std::array<int3, 8> bestTile;
		std::array<int, 8> bestDistSq;
		std::array<int, 8> bestMoveRemains;
		bestTile.fill(int3(-1, -1, -1));
		bestDistSq.fill(-1);
		bestMoveRemains.fill(0);

		const int3 mapSize = cb->getMapSize();
		for(int z = 0; z < mapSize.z; ++z)
			for(int x = 0; x < mapSize.x; ++x)
				for(int y = 0; y < mapSize.y; ++y)
				{
					const int3 tile(x, y, z);
					if(tile == from)
						continue;
					const CGPathNode * node = heroPaths.getPathInfo(tile);
					if(node == nullptr || !node->reachable() || node->turns != 0)
						continue;
					// Only plain standable destinations as MOVE targets; object/visit
					// tiles are handled by VISIT_OBJECT (T0b), not a bare move.
					if(node->action != EPathNodeAction::NORMAL
						&& node->action != EPathNodeAction::EMBARK
						&& node->action != EPathNodeAction::DISEMBARK)
						continue;
					const int oct = octantOf(x - from.x, y - from.y);
					if(oct < 0)
						continue;
					const int distSq = (x - from.x) * (x - from.x) + (y - from.y) * (y - from.y);
					if(distSq > bestDistSq[oct])
					{
						bestDistSq[oct] = distSq;
						bestTile[oct] = tile;
						bestMoveRemains[oct] = node->moveRemains;
					}
				}

		for(int oct = 0; oct < 8; ++oct)
		{
			if(emitted >= MAX_MOVE_OPTIONS)
				break;
			if(bestDistSq[oct] < 0)
				continue;
			const int3 dst = bestTile[oct];
			JsonNode option;
			const int heroIdNum = hero->id.getNum();
			option["option_id"].String() = "move_h_" + std::to_string(heroIdNum) + "_x_" + std::to_string(dst.x) + "_y_" + std::to_string(dst.y);
			option["hero_id"].String() = heroToken(heroIdNum);
			option["to_x"].Integer() = dst.x;
			option["to_y"].Integer() = dst.y;
			option["turns_to_reach"].Integer() = 0;
			option["move_remains"].Integer() = bestMoveRemains[oct];
			std::string guardName;
			long long guardCount = 0;
			const long long guardPower = guardPowerAt(dst, guardName, guardCount);
			if(guardPower > 0)
			{
				option["guard_power"].Integer() = guardPower;
				option["guard_count"].Integer() = guardCount;
				if(!guardName.empty())
					option["guard_name"].String() = guardName;
			}
			moveOptions.Vector().push_back(option);
			++emitted;
		}

		// T0b: also offer reachable VISIBLE OBJECTS as move destinations (closest
		// first, multi-turn allowed). This lets the hero pursue purposeful goals
		// (mines, dwellings, resources, enemy towns/heroes) over several turns instead
		// of only exploring adjacent fog -- the execution path-traverses toward the
		// object and visits it on arrival. The model maps these coords to entries in
		// visible_state.map_objects_visible. Multi-turn targets (turns > 0) advance the
		// hero as far as movement allows each turn and are re-offered until reached.
		std::vector<std::tuple<int, int, int3, bool>> objTargets; // (turns, moveRemains, pos, priority)
		for(const auto * obj : visibleObjects)
		{
			if(obj == nullptr)
				continue;
			// T1: skip already-owned objects (our flagged mines/dwellings) — re-visiting
			// yields nothing. EXCEPTION: our own town while it holds an uncollected
			// garrison. The hero legitimately needs to return there to pick those troops
			// up; the model recognizes this in its plan ("move onto town to MANAGE_ARMY
			// merging all N Imps") but, without this move being offered, has no way to act
			// on it — so its recruited army strands at home (observed: 7500-power garrison
			// idle vs a 2256 roaming hero). Once the hero arrives, auto-collect / MANAGE_ARMY
			// grabs the garrison.
			if(obj->tempOwner == playerID)
			{
				const auto * ownTown = dynamic_cast<const CGTownInstance *>(obj);
				const bool collectableGarrison =
					ownTown != nullptr
					&& ownTown->getUpperArmy() != nullptr
					&& ownTown->getUpperArmy()->stacksCount() > 0;
				if(!collectableGarrison)
					continue;
			}
			const int3 opos = obj->visitablePos();
			if(opos == from)
				continue;
			const CGPathNode * onode = heroPaths.getPathInfo(opos);
			{
				const char * dbgFlag = std::getenv("ARENA_DEBUG_MOVE");
				if(dbgFlag != nullptr && dbgFlag[0] == '1'
					&& obj->tempOwner.isValidPlayer() && obj->tempOwner != playerID
					&& (dynamic_cast<const CGTownInstance *>(obj) != nullptr
						|| dynamic_cast<const CGHeroInstance *>(obj) != nullptr))
				{
					std::ofstream dbg("/tmp/arena_move_debug.log", std::ios::app);
					dbg << "offer? enemy " << obj->getObjectName() << " at " << opos.toString()
						<< " hero at " << from.toString()
						<< " node=" << (onode == nullptr ? "null" : "ok")
						<< " turns=" << (onode ? static_cast<int>(onode->turns) : -1)
						<< " action=" << (onode ? static_cast<int>(onode->action) : -1) << "\n";
				}
			}
			if(onode == nullptr || !onode->reachable())
				continue; // no path to this object at all
			// A visible ENEMY town or hero is a win-condition target: it must never be
			// crowded out of the MOVE options by nearby ordinary targets (mines,
			// dwellings) under the closest-first cap ordering — which is exactly what
			// happened during a long cross-map approach. Priority targets bypass the
			// MAX_MOVE_OPTIONS cap (bounded anyway by the visible-object caps).
			const bool enemyOwned = obj->tempOwner.isValidPlayer() && obj->tempOwner != playerID;
			const bool priorityTarget = enemyOwned
				&& (dynamic_cast<const CGTownInstance *>(obj) != nullptr
					|| dynamic_cast<const CGHeroInstance *>(obj) != nullptr);
			objTargets.emplace_back(static_cast<int>(onode->turns), onode->moveRemains, opos, priorityTarget);
		}
		std::sort(objTargets.begin(), objTargets.end(),
			[](const auto & a, const auto & b) { return std::get<0>(a) < std::get<0>(b); });

		for(const auto & target : objTargets)
		{
			if(!std::get<3>(target) && emitted >= MAX_MOVE_OPTIONS)
				continue; // cap ordinary targets only; enemy towns/heroes always emit
			const int3 opos = std::get<2>(target);
			bool duplicate = false;
			for(int oct = 0; oct < 8; ++oct)
			{
				if(bestDistSq[oct] >= 0 && bestTile[oct] == opos)
				{
					duplicate = true;
					break;
				}
			}
			if(duplicate)
				continue; // already offered as an exploration tile

			JsonNode option;
			const int heroIdNum = hero->id.getNum();
			option["option_id"].String() = "move_h_" + std::to_string(heroIdNum) + "_x_" + std::to_string(opos.x) + "_y_" + std::to_string(opos.y);
			option["hero_id"].String() = heroToken(heroIdNum);
			option["to_x"].Integer() = opos.x;
			option["to_y"].Integer() = opos.y;
			option["turns_to_reach"].Integer() = std::get<0>(target);
			option["move_remains"].Integer() = std::get<1>(target);
			std::string guardName;
			long long guardCount = 0;
			const long long guardPower = guardPowerAt(opos, guardName, guardCount);
			if(guardPower > 0)
			{
				option["guard_power"].Integer() = guardPower;
				option["guard_count"].Integer() = guardCount;
				if(!guardName.empty())
					option["guard_name"].String() = guardName;
			}
			moveOptions.Vector().push_back(option);
			++emitted;
		}

		// No early break at the cap: later heroes still get their priority
		// (enemy town/hero) targets emitted; their ordinary options are capped
		// by the per-emission checks above.
	}

	if(!moveOptions.Vector().empty())
	{
		moveGroup["options"] = moveOptions;
		legalActions.Vector().push_back(moveGroup);
	}

	JsonNode buildGroup;
	buildGroup["type"].String() = "BUILD";
	JsonNode buildOptions;
	buildOptions.setType(JsonNode::JsonType::DATA_VECTOR);
	int buildEmitted = 0;
	for(const auto * town : towns)
	{
		if(town == nullptr)
			continue;

		for(const auto & buildingEntry : town->getTown()->buildings)
		{
			if(buildEmitted >= MAX_BUILD_OPTIONS)
				break;
			const auto * building = buildingEntry.second.get();
			if(building == nullptr)
				continue;
			if(cb->canBuildStructure(town, buildingEntry.first) != EBuildingState::ALLOWED)
				continue;

			JsonNode option;
			option["option_id"].String() = "build_t_" + std::to_string(town->id.getNum()) + "_b_" + std::to_string(buildingEntry.first.getNum());
			option["town_id"].String() = townToken(town->id.getNum());
			option["building"].String() = building->getJsonKey();
			option["building_name"].String() = building->getNameTranslated();
			JsonNode costNode;
			pushBuildingCost(costNode, building->resources);
			option["cost"] = costNode;
			buildOptions.Vector().push_back(option);
			++buildEmitted;
		}
		if(buildEmitted >= MAX_BUILD_OPTIONS)
			break;
	}
	if(!buildOptions.Vector().empty())
	{
		buildGroup["options"] = buildOptions;
		legalActions.Vector().push_back(buildGroup);
	}

	JsonNode recruitGroup;
	recruitGroup["type"].String() = "RECRUIT";
	JsonNode recruitOptions;
	recruitOptions.setType(JsonNode::JsonType::DATA_VECTOR);
	int recruitEmitted = 0;
	const auto resources = cb->getResourceAmount();
	for(const auto * town : towns)
	{
		if(town == nullptr)
			continue;

		for(size_t level = 0; level < town->creatures.size(); ++level)
		{
			if(recruitEmitted >= MAX_RECRUIT_OPTIONS)
				break;
			const auto & entry = town->creatures[level];
			if(entry.first <= 0 || entry.second.empty())
				continue;
			const auto creatureId = entry.second.front();
			const auto * creature = creatureId.toCreature();
			if(creature == nullptr)
				continue;

			const int available = static_cast<int>(entry.first);
			const int affordable = creature->maxAmount(resources);
			const int recruitCount = std::min(available, affordable);
			if(recruitCount <= 0)
				continue;

			JsonNode option;
			option["option_id"].String() = "recruit_t_" + std::to_string(town->id.getNum())
				+ "_c_" + std::to_string(creatureId.getNum())
				+ "_l_" + std::to_string(static_cast<int>(level))
				+ "_n_" + std::to_string(recruitCount);
			option["town_id"].String() = townToken(town->id.getNum());
			option["unit"].String() = creature->getJsonKey();
			option["unit_name"].String() = creature->getNameSingularTranslated();
			option["count"].Integer() = recruitCount;
			recruitOptions.Vector().push_back(option);
			++recruitEmitted;
		}
		if(recruitEmitted >= MAX_RECRUIT_OPTIONS)
			break;
	}
	if(!recruitOptions.Vector().empty())
	{
		recruitGroup["options"] = recruitOptions;
		legalActions.Vector().push_back(recruitGroup);
	}

	JsonNode recruitHeroGroup;
	recruitHeroGroup["type"].String() = "RECRUIT_HERO";
	JsonNode recruitHeroOptions;
	recruitHeroOptions.setType(JsonNode::JsonType::DATA_VECTOR);
	int recruitHeroEmitted = 0;
	const int heroCount = cb->getHeroCount(playerID, true);
	const int onMapCap = cb->getSettings().getInteger(EGameSettings::HEROES_PER_PLAYER_ON_MAP_CAP);
	const int totalCap = cb->getSettings().getInteger(EGameSettings::HEROES_PER_PLAYER_TOTAL_CAP);
	const bool heroCapReached = heroCount >= onMapCap || heroCount >= totalCap;
	const bool canAffordHero = cb->getResourceAmount(EGameResID::GOLD) >= GameConstants::HERO_GOLD_COST;
	if(canAffordHero && !heroCapReached)
	{
		for(const auto * town : towns)
		{
			if(town == nullptr)
				continue;
			if(!town->hasBuilt(BuildingID::TAVERN))
				continue;
			if(town->getVisitingHero() && town->getUpperArmy() && town->getUpperArmy()->stacksCount() > 0)
				continue;

			const auto offeredHeroes = cb->getAvailableHeroes(town);
			for(const auto * offeredHero : offeredHeroes)
			{
				if(recruitHeroEmitted >= MAX_RECRUIT_HERO_OPTIONS)
					break;
				if(offeredHero == nullptr || !offeredHero->getHeroTypeID().isValid())
					continue;

				const int townIdNum = town->id.getNum();
				const int heroTypeIdNum = offeredHero->getHeroTypeID().getNum();
				JsonNode option;
				option["option_id"].String() = "recruit_hero_t_" + std::to_string(townIdNum)
					+ "_h_" + std::to_string(heroTypeIdNum);
				option["town_id"].String() = townToken(townIdNum);
				option["hero_type_id"].Integer() = heroTypeIdNum;
				option["hero_name"].String() = offeredHero->getNameTranslated();
				if(const auto * heroType = offeredHero->getHeroTypeID().toHeroType())
					option["hero"].String() = heroType->getJsonKey();
				recruitHeroOptions.Vector().push_back(option);
				++recruitHeroEmitted;
			}
			if(recruitHeroEmitted >= MAX_RECRUIT_HERO_OPTIONS)
				break;
		}
	}
	if(!recruitHeroOptions.Vector().empty())
	{
		recruitHeroGroup["options"] = recruitHeroOptions;
		legalActions.Vector().push_back(recruitHeroGroup);
	}

	// T2 Phase B: army management. When a friendly hero is VISITING a friendly town,
	// let the model move stacks between the town garrison and the hero. The key gap
	// this closes: RECRUIT lands creatures in the town garrison, so a roaming hero
	// fields a weak army while strength piles up uselessly at home (T2 run: hero 1435
	// vs garrison 1550). "garrison_to_hero" lets the hero collect what it recruited;
	// "hero_to_garrison" lets it drop weak troops (never emptying the hero).
	JsonNode manageGroup;
	manageGroup["type"].String() = "MANAGE_ARMY";
	JsonNode manageOptions;
	manageOptions.setType(JsonNode::JsonType::DATA_VECTOR);
	int manageEmitted = 0;
	for(const auto * town : towns)
	{
		if(town == nullptr)
			continue;
		const CGHeroInstance * visiting = town->getVisitingHero();
		if(visiting == nullptr || visiting->tempOwner != playerID)
			continue;
		const int townIdNum = town->id.getNum();
		const int heroIdNum = visiting->id.getNum();

		auto pushManageOption = [&](int direction, const CCreatureSet & srcArmy, const SlotID & slotId)
		{
			const CCreature * creature = srcArmy.getCreature(slotId);
			if(creature == nullptr)
				return;
			JsonNode option;
			option["option_id"].String() = "manage_t_" + std::to_string(townIdNum)
				+ "_h_" + std::to_string(heroIdNum)
				+ "_dir_" + std::to_string(direction)
				+ "_s_" + std::to_string(slotId.getNum());
			option["town_id"].String() = townToken(townIdNum);
			option["hero_id"].String() = heroToken(heroIdNum);
			option["direction"].String() = direction == 0 ? "garrison_to_hero" : "hero_to_garrison";
			option["unit"].String() = creature->getJsonKey();
			option["unit_name"].String() = creature->getNameSingularTranslated();
			option["count"].Integer() = srcArmy.getStackCount(slotId);
			manageOptions.Vector().push_back(option);
			++manageEmitted;
		};

		// Pickup: each garrison stack -> hero.
		for(const auto & slot : town->Slots())
		{
			if(manageEmitted >= MAX_MANAGE_ARMY_OPTIONS)
				break;
			pushManageOption(0, *town, slot.first);
		}
		// Deposit: each hero stack -> garrison, but never offer a move that empties the
		// hero of its last stack (the engine forbids it and it would be suicidal anyway).
		if(visiting->stacksCount() >= 2)
		{
			for(const auto & slot : visiting->Slots())
			{
				if(manageEmitted >= MAX_MANAGE_ARMY_OPTIONS)
					break;
				pushManageOption(1, *visiting, slot.first);
			}
		}
		if(manageEmitted >= MAX_MANAGE_ARMY_OPTIONS)
			break;
	}
	if(!manageOptions.Vector().empty())
	{
		manageGroup["options"] = manageOptions;
		legalActions.Vector().push_back(manageGroup);
	}

	payload["legal_actions"] = legalActions;
	payload["decision_budget"]["timeout_ms"].Integer() = std::max(100, std::min(decisionTimeoutMs, turnRemainingMs));
	payload["decision_budget"]["turn_remaining_ms"].Integer() = std::max(0, turnRemainingMs);
	payload["decision_budget"]["turn_timeout_ms"].Integer() = turnTimeBudgetMs;
	payload["decision_budget"]["max_actions_per_turn"].Integer() = maxActionsPerTurn;
	payload["decision_budget"]["action_index"].Integer() = actionIndex;
	payload["decision_budget"]["max_output_tokens"].Integer() = 320;
	return payload;
}

bool CArenaAI::sendEnvelope(const std::string & msgType, const JsonNode & payload, JsonNode & responseOut)
{
	if(!bridgeEnabled || socketPath.empty())
		return false;
	if(socketPath.size() >= sizeof(sockaddr_un::sun_path))
		return false;

	JsonNode envelope;
	envelope["schema_version"].String() = BRIDGE_SCHEMA_VERSION;
	envelope["match_id"].String() = matchId;
	envelope["side"].String() = sideName;
	envelope["seq"].Integer() = static_cast<si64>(seqCounter++);
	envelope["msg_type"].String() = msgType;
	envelope["ts_utc"].String() = nowUtcSeconds();
	envelope["payload"] = payload;

	const std::string requestLine = envelope.toCompactString() + "\n";

	int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0)
		return false;

	timeval tv{};
	tv.tv_sec = decisionTimeoutMs / 1000;
	tv.tv_usec = (decisionTimeoutMs % 1000) * 1000;
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	sockaddr_un address{};
	address.sun_family = AF_UNIX;
	std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", socketPath.c_str());
	if(::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
	{
		::close(fd);
		return false;
	}

	const char * dataPtr = requestLine.c_str();
	size_t remaining = requestLine.size();
	while(remaining > 0)
	{
		const ssize_t sent = ::send(fd, dataPtr, remaining, 0);
		if(sent <= 0)
		{
			::close(fd);
			return false;
		}
		dataPtr += sent;
		remaining -= static_cast<size_t>(sent);
	}

	std::string responseLine;
	char buffer[2048];
	for(;;)
	{
		const ssize_t got = ::recv(fd, buffer, sizeof(buffer), 0);
		if(got <= 0)
			break;
		responseLine.append(buffer, buffer + got);
		if(responseLine.find('\n') != std::string::npos)
			break;
	}
	::close(fd);

	if(responseLine.empty())
		return false;
	const size_t newlinePos = responseLine.find('\n');
	if(newlinePos != std::string::npos)
		responseLine.resize(newlinePos);

	try
	{
		JsonParsingSettings parserSettings;
		parserSettings.strict = false;
		JsonNode parsed(responseLine.c_str(), responseLine.size(), parserSettings, "arenaai_bridge_response");
		if(!parsed.isStruct())
			return false;
		if(parsed["msg_type"].isString() && parsed["msg_type"].String() == "error")
			return false;
		responseOut = parsed;
		return true;
	}
	catch(...)
	{
		return false;
	}
}

std::optional<int> CArenaAI::parseOptionValue(const std::string & token) const
{
	if(token.empty())
		return std::nullopt;

	auto parseInt = [](const std::string & value) -> std::optional<int>
	{
		try
		{
			return std::stoi(value);
		}
		catch(...)
		{
			return std::nullopt;
		}
	};

	if(token.rfind("opt_", 0) == 0)
		return parseInt(token.substr(4));

	if(const auto value = parseInt(token))
		return value;

	const size_t pos = token.find_last_of('_');
	if(pos != std::string::npos && pos + 1 < token.size())
		return parseInt(token.substr(pos + 1));

	return std::nullopt;
}

std::optional<int> CArenaAI::jsonInt(const JsonNode & node) const
{
	if(node.getType() == JsonNode::JsonType::DATA_INTEGER)
		return static_cast<int>(node.Integer());
	if(node.getType() == JsonNode::JsonType::DATA_FLOAT)
		return static_cast<int>(node.Float());
	return std::nullopt;
}

std::optional<int> CArenaAI::parseTaggedOptionValue(const std::string & token, const std::string & marker) const
{
	if(token.empty() || marker.empty())
		return std::nullopt;
	const size_t pos = token.find(marker);
	if(pos == std::string::npos)
		return std::nullopt;

	size_t begin = pos + marker.size();
	if(begin >= token.size())
		return std::nullopt;

	size_t end = begin;
	if(token[end] == '-')
		++end;
	while(end < token.size() && std::isdigit(static_cast<unsigned char>(token[end])))
		++end;
	if(end == begin || (token[begin] == '-' && end == begin + 1))
		return std::nullopt;
	try
	{
		return std::stoi(token.substr(begin, end - begin));
	}
	catch(...)
	{
		return std::nullopt;
	}
}

const CGHeroInstance * CArenaAI::findHeroById(int heroId) const
{
	for(const auto * hero : cb->getHeroesInfo())
	{
		if(hero != nullptr && hero->id.getNum() == heroId)
			return hero;
	}
	return nullptr;
}

const CGTownInstance * CArenaAI::findTownById(int townId) const
{
	for(const auto * town : cb->getTownsInfo(true))
	{
		if(town != nullptr && town->id.getNum() == townId)
			return town;
	}
	return nullptr;
}

bool CArenaAI::applyTurnResponse(const JsonNode & responsePayload)
{
	if(!responsePayload.isStruct())
		return false;

	const JsonNode & selectedAction = responsePayload["selected_action"];
	const std::string actionType = selectedActionType(responsePayload);
	if(actionType.empty())
		return false;

	if(actionType == "END_TURN")
		return true;

	std::string optionId;
	if(selectedAction.isStruct() && selectedAction["option_id"].isString())
		optionId = selectedAction["option_id"].String();

	if(actionType == "MOVE_HERO")
	{
		std::optional<int> heroId;
		if(selectedAction.isStruct() && selectedAction["hero_id"].isString())
			heroId = parseOptionValue(selectedAction["hero_id"].String());
		if(!heroId && !optionId.empty())
			heroId = parseTaggedOptionValue(optionId, "h_");

		std::optional<int> toX = selectedAction.isStruct() ? jsonInt(selectedAction["to_x"]) : std::nullopt;
		std::optional<int> toY = selectedAction.isStruct() ? jsonInt(selectedAction["to_y"]) : std::nullopt;
		if(!toX && !optionId.empty())
			toX = parseTaggedOptionValue(optionId, "_x_");
		if(!toY && !optionId.empty())
			toY = parseTaggedOptionValue(optionId, "_y_");

		if(!heroId || !toX || !toY)
			return false;

		const CGHeroInstance * hero = findHeroById(*heroId);
		if(hero == nullptr)
			return false;

		const int3 destination(*toX, *toY, hero->visitablePos().z);

		const bool reached = executeHeroMoveTo(hero, destination);

		// T1 persistent intent: if the model aimed at a still-distant unowned object
		// and we did not arrive this turn, remember the goal so advanceTravelGoals()
		// keeps moving toward it next turn instead of letting the hero wander off.
		const int heroNum = hero->id.getNum();
		if(!reached && unownedObjectAt(destination) != nullptr)
			heroTravelGoals[heroNum] = destination;
		else
			heroTravelGoals.erase(heroNum);
		return true;
	}

	if(actionType == "BUILD")
	{
		std::optional<int> townId;
		if(selectedAction.isStruct() && selectedAction["town_id"].isString())
			townId = parseOptionValue(selectedAction["town_id"].String());
		if(!townId && !optionId.empty())
			townId = parseTaggedOptionValue(optionId, "_t_");
		if(!townId)
			return false;

		const CGTownInstance * town = findTownById(*townId);
		if(town == nullptr)
			return false;

		std::optional<int> buildingId = selectedAction.isStruct() ? jsonInt(selectedAction["building_id"]) : std::nullopt;
		if(!buildingId && !optionId.empty())
			buildingId = parseTaggedOptionValue(optionId, "_b_");

		if(!buildingId && selectedAction.isStruct() && selectedAction["building"].isString())
		{
			const std::string requested = selectedAction["building"].String();
			for(const auto & entry : town->getTown()->buildings)
			{
				const auto * building = entry.second.get();
				if(building == nullptr)
					continue;
				if(building->getJsonKey() == requested || building->getNameTranslated() == requested)
				{
					buildingId = entry.first.getNum();
					break;
				}
			}
		}
		if(!buildingId)
			return false;

		const BuildingID buildType(*buildingId);
		if(cb->canBuildStructure(town, buildType) != EBuildingState::ALLOWED)
			return false;
		return cb->buildBuilding(town, buildType);
	}

	if(actionType == "RECRUIT")
	{
		std::optional<int> townId;
		if(selectedAction.isStruct() && selectedAction["town_id"].isString())
			townId = parseOptionValue(selectedAction["town_id"].String());
		if(!townId && !optionId.empty())
			townId = parseTaggedOptionValue(optionId, "_t_");
		if(!townId)
			return false;

		const CGTownInstance * town = findTownById(*townId);
		if(town == nullptr)
			return false;

		std::optional<int> creatureId = selectedAction.isStruct() ? jsonInt(selectedAction["unit_id"]) : std::nullopt;
		if(!creatureId && !optionId.empty())
			creatureId = parseTaggedOptionValue(optionId, "_c_");

		std::optional<int> levelHint = !optionId.empty() ? parseTaggedOptionValue(optionId, "_l_") : std::nullopt;
		std::optional<int> requestedCount = selectedAction.isStruct() ? jsonInt(selectedAction["count"]) : std::nullopt;
		if(!requestedCount && !optionId.empty())
			requestedCount = parseTaggedOptionValue(optionId, "_n_");

		std::string unitToken;
		if(selectedAction.isStruct() && selectedAction["unit"].isString())
			unitToken = selectedAction["unit"].String();

		int matchedLevel = -1;
		int maxLegalCount = 0;
		if(creatureId)
		{
			const CreatureID creatureType(*creatureId);
			const auto * creature = creatureType.toCreature();
			if(creature == nullptr)
				return false;
			if(unitToken.empty())
				unitToken = creature->getJsonKey();

			for(size_t level = 0; level < town->creatures.size(); ++level)
			{
				const auto & entry = town->creatures[level];
				if(entry.first <= 0)
					continue;
				if(levelHint && static_cast<int>(level) != *levelHint)
					continue;
				if(std::find(entry.second.begin(), entry.second.end(), creatureType) == entry.second.end())
					continue;

				const int available = static_cast<int>(entry.first);
				const int affordable = creature->maxAmount(cb->getResourceAmount());
				maxLegalCount = std::min(available, affordable);
				matchedLevel = static_cast<int>(level);
				break;
			}
		}
		else
		{
			for(size_t level = 0; level < town->creatures.size(); ++level)
			{
				const auto & entry = town->creatures[level];
				if(entry.first <= 0 || entry.second.empty())
					continue;
				if(levelHint && static_cast<int>(level) != *levelHint)
					continue;

				const auto candidateId = entry.second.front();
				const auto * candidate = candidateId.toCreature();
				if(candidate == nullptr)
					continue;
				if(!unitToken.empty() && unitToken != candidate->getJsonKey() && unitToken != candidate->getNameSingularTranslated())
					continue;

				const int available = static_cast<int>(entry.first);
				const int affordable = candidate->maxAmount(cb->getResourceAmount());
				const int legalCount = std::min(available, affordable);
				if(legalCount <= 0)
					continue;
				creatureId = candidateId.getNum();
				maxLegalCount = legalCount;
				matchedLevel = static_cast<int>(level);
				break;
			}
		}

		if(!creatureId || matchedLevel < 0 || maxLegalCount <= 0)
			return false;

		int finalCount = requestedCount.value_or(maxLegalCount);
		finalCount = std::max(1, std::min(finalCount, maxLegalCount));
		cb->recruitCreatures(town, town, CreatureID(*creatureId), static_cast<ui32>(finalCount), matchedLevel);
		return true;
	}

	if(actionType == "RECRUIT_HERO")
	{
		std::optional<int> townId;
		if(selectedAction.isStruct() && selectedAction["town_id"].isString())
			townId = parseOptionValue(selectedAction["town_id"].String());
		if(!townId && !optionId.empty())
			townId = parseTaggedOptionValue(optionId, "_t_");
		if(!townId)
			return false;

		const CGTownInstance * town = findTownById(*townId);
		if(town == nullptr)
			return false;
		if(!town->hasBuilt(BuildingID::TAVERN))
			return false;
		if(cb->getResourceAmount(EGameResID::GOLD) < GameConstants::HERO_GOLD_COST)
			return false;

		std::optional<int> heroTypeId = selectedAction.isStruct() ? jsonInt(selectedAction["hero_type_id"]) : std::nullopt;
		if(!heroTypeId && !optionId.empty())
			heroTypeId = parseTaggedOptionValue(optionId, "_h_");

		std::string heroTokenValue;
		if(selectedAction.isStruct() && selectedAction["hero"].isString())
			heroTokenValue = selectedAction["hero"].String();

		const auto offeredHeroes = cb->getAvailableHeroes(town);
		const CGHeroInstance * selectedHero = nullptr;
		for(const auto * offeredHero : offeredHeroes)
		{
			if(offeredHero == nullptr || !offeredHero->getHeroTypeID().isValid())
				continue;
			const int offeredHeroTypeId = offeredHero->getHeroTypeID().getNum();
			if(heroTypeId && offeredHeroTypeId == *heroTypeId)
			{
				selectedHero = offeredHero;
				break;
			}
			if(!heroTokenValue.empty())
			{
				const auto * heroType = offeredHero->getHeroTypeID().toHeroType();
				if(heroType && (heroType->getJsonKey() == heroTokenValue || offeredHero->getNameTranslated() == heroTokenValue))
				{
					selectedHero = offeredHero;
					break;
				}
			}
		}
		if(selectedHero == nullptr)
			return false;

		cb->recruitHero(town, selectedHero);
		return true;
	}

	if(actionType == "MANAGE_ARMY")
	{
		std::optional<int> townId = (!optionId.empty()) ? parseTaggedOptionValue(optionId, "_t_") : std::nullopt;
		std::optional<int> heroId = (!optionId.empty()) ? parseTaggedOptionValue(optionId, "_h_") : std::nullopt;
		std::optional<int> direction = (!optionId.empty()) ? parseTaggedOptionValue(optionId, "_dir_") : std::nullopt;
		std::optional<int> srcSlot = (!optionId.empty()) ? parseTaggedOptionValue(optionId, "_s_") : std::nullopt;
		if(!townId && selectedAction.isStruct() && selectedAction["town_id"].isString())
			townId = parseOptionValue(selectedAction["town_id"].String());
		if(!heroId && selectedAction.isStruct() && selectedAction["hero_id"].isString())
			heroId = parseOptionValue(selectedAction["hero_id"].String());
		if(!townId || !heroId || !direction || !srcSlot)
			return false;

		const CGTownInstance * town = findTownById(*townId);
		const CGHeroInstance * hero = findHeroById(*heroId);
		if(town == nullptr || hero == nullptr)
			return false;
		// Both armies must be co-located (hero visiting the town) for a transfer.
		if(town->getVisitingHero() != hero)
			return false;

		const SlotID slot(*srcSlot);
		if(*direction == 0)
		{
			// garrison -> hero. Do NOT use bulkMoveArmy here: it deliberately leaves one
			// unit behind so it never empties the SOURCE army (correct for hero<->hero
			// exchange — a hero may not be left with zero troops). But a town garrison MAY
			// legally be empty, so against a garrison bulkMoveArmy strands the last creature
			// of a stack forever; the model then loops on "pick up the remaining unit" and
			// the whole game stalls (observed: army frozen for 18 turns). Move the entire
			// stack into the hero's matching slot (merge) or a free slot (swap) instead.
			const CCreature * cre = town->getCreature(slot);
			if(cre == nullptr)
				return false;
			const SlotID dstSlot = hero->getSlotFor(cre);
			if(!dstSlot.validSlot())
				return false; // hero army full with no matching slot — cannot pick up
			cb->mergeOrSwapStacks(town, hero, slot, dstSlot); // garrison stack -> hero, in full
		}
		else
		{
			// hero -> garrison: bulkMoveArmy's leave-one rule is DESIRABLE here (never empty
			// the hero), so keep it for deposits.
			cb->bulkMoveArmy(hero->id, town->id, slot);
		}
		return true;
	}

	return false;
}

std::string CArenaAI::selectedActionType(const JsonNode & responsePayload) const
{
	if(!responsePayload.isStruct())
		return "";
	const JsonNode & selectedAction = responsePayload["selected_action"];
	if(selectedAction.isStruct() && selectedAction["type"].isString())
		return selectedAction["type"].String();
	if(responsePayload["selected_action_type"].isString())
		return responsePayload["selected_action_type"].String();
	return "";
}

int CArenaAI::chooseSelectionViaBridge(QueryID queryID, const std::string & queryType, const std::vector<int> & options, int defaultSelection, const std::string & queryText)
{
	if(!bridgeEnabled || options.empty())
		return defaultSelection;

	JsonNode payload;
	payload["query_id"].String() = std::to_string(queryID.getNum());
	payload["query_type"].String() = queryType;
	payload["acting_side"].String() = sideName;
	if(!queryText.empty())
		payload["query_text"].String() = queryText;

	JsonNode optionNodes;
	optionNodes.setType(JsonNode::JsonType::DATA_VECTOR);
	for(const int option : options)
	{
		JsonNode entry;
		entry["option_id"].String() = "opt_" + std::to_string(option);
		entry["safe"].Bool() = true;
		optionNodes.Vector().push_back(entry);
	}
	payload["options"] = optionNodes;

	JsonNode response;
	if(!sendEnvelope("query_request", payload, response))
		return defaultSelection;

	const JsonNode & responsePayload = response["payload"];
	if(!responsePayload.isStruct())
		return defaultSelection;

	std::optional<int> selected;
	if(responsePayload["selected_option_id"].isString())
		selected = parseOptionValue(responsePayload["selected_option_id"].String());
	else
		selected = jsonInt(responsePayload["selected_option_id"]);

	if(!selected)
		return defaultSelection;
	if(std::find(options.begin(), options.end(), *selected) == options.end())
		return defaultSelection;
	return *selected;
}

void CArenaAI::yourTurn(QueryID queryID)
{
	// Drive the turn from a worker thread. The network thread MUST stay free to
	// deliver battle packs (battleStart/activeStack/battleEnd) and the
	// PackageApplied acks that unblock waitTillRealize'd requests. Running the loop
	// here (on the network thread) would deadlock the instant a blocking request is
	// issued — the thread would wait on a pack only it can process.
	if(turnThread.joinable())
		turnThread.join();
	turnThread = std::thread(&CArenaAI::runTurn, this, queryID);
}

void CArenaAI::runTurn(QueryID queryID)
// Function-try-block: when the game ends (e.g. our own move gets red's last hero
// killed -> elimination -> game-end teardown), finishGameplay() interrupts our
// in-flight sendRequest, which throws TerminationRequestedException up this worker
// thread. Without catching it the exception escapes the thread -> std::terminate
// (process aborts with the winner already decided). Swallow it for a clean exit.
try
{
	if(queryID.getNum() >= 0)
		answerQuery(queryID, 0);

	// Hold a shared lock on the game state for the whole turn, mirroring Nullkiller's
	// makeTurn. This both makes our state reads safe against the network thread's pack
	// application AND satisfies waitTillRealize's contract: sendRequest releases this
	// shared lock while it waits for the ack (makeUnlockSharedGuard) so the network
	// thread can take the exclusive lock to apply packs. Without holding it here, that
	// unlock_shared underflows the reader count and the network thread deadlocks.
	std::shared_lock<std::shared_mutex> gsLock(CGameState::mutex);

	if(bridgeEnabled)
	{
		// Collect any garrison troops into a visiting hero FIRST, so recruited
		// reinforcements join the field army before the hero ventures out (the model
		// recruits remotely but often never returns to collect — observed a 7500-power
		// garrison stranded idle vs a 2256-power roaming hero).
		autoCollectGarrisons();
		// T1: before asking the model, continue any standing travel goals so a
		// multi-turn "go take that object" intent actually completes across turns
		// rather than the hero re-deciding each step and oscillating in place.
		advanceTravelGoals();

		const auto turnStart = std::chrono::steady_clock::now();
		for(int actionIndex = 1; actionIndex <= maxActionsPerTurn; ++actionIndex)
		{
			if(aborting)
				break;

			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - turnStart
			).count();
			const int elapsedMs = static_cast<int>(elapsed);
			const int turnRemainingMs = turnTimeBudgetMs - elapsedMs;
			if(turnRemainingMs < 100)
				break;

			JsonNode response;
			JsonNode payload = buildTurnRequestPayload(queryID, actionIndex, turnRemainingMs);
			if(!sendEnvelope("turn_request", payload, response))
				break;

			const JsonNode & responsePayload = response["payload"];
			const std::string actionType = selectedActionType(responsePayload);
			if(actionType.empty())
				break;
			if(actionType == "END_TURN")
				break;
			if(!applyTurnResponse(responsePayload))
				break;

			// A move can start a battle; wait for it to auto-resolve before issuing
			// the next action so the bridge sees a clean post-battle state (and so we
			// don't fire actions the server would reject while a CBattleQuery is open).
			waitForBattles();
		}
	}

	if(!aborting)
		cb->endTurn();
}
catch(const TerminationRequestedException &)
{
	// Game ended while this turn was in flight; the worker is being joined. Exit cleanly.
	logAi->info("ArenaAI: turn interrupted by game shutdown");
}

void CArenaAI::waitForBattles()
{
	// Release the game-state shared lock for the duration of the wait so the network
	// thread can take the exclusive lock and drive the battle (battleStart/activeStack/
	// battleEnd) to completion. Mirrors Nullkiller's waitTillFree.
	auto gsUnlock = vstd::makeUnlockSharedGuard(CGameState::mutex);
	std::unique_lock<std::mutex> lock(battleMx);
	// Bounded so a wedged battle can never hang the worker indefinitely.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
	battleCv.wait_until(lock, deadline, [this]() {
		return battlesInProgress == 0 || aborting.load();
	});
}

bool CArenaAI::executeHeroMoveTo(const CGHeroInstance * hero, const int3 & destination,
	bool stopBeforeCombat, bool * blockedByGuard)
{
	// Execute along the engine's path. A bare moveHero(int3) sends a one-tile,
	// source-less path which the server rejects ("destination tile is blocked"),
	// so heroes never actually moved. Compute the real path and send the sequence
	// of tiles reachable this turn (forward order, excluding the start) in one pack.
	CPathsInfo heroPaths(cb->getMapSize(), hero);
	auto pathConfig = std::make_shared<SingleHeroPathfinderConfig>(heroPaths, *cb, hero);
	cb->calculatePaths(pathConfig);

	CGPath path;
	std::vector<int3> tiles;
	bool reachedDestination = false;
	{
		// New move, new intent: drop any stale exit choice from a previous move.
		std::lock_guard<std::mutex> lock(teleportExitMx);
		plannedTeleportExit = int3(-1, -1, -1);
	}
	const bool debugMove = []() {
		const char * flag = std::getenv("ARENA_DEBUG_MOVE");
		return flag != nullptr && flag[0] == '1';
	}();
	if(heroPaths.getPath(path, destination))
	{
		// Record which teleporter exit this path intends to come out of (the first
		// teleport-action node), so the TeleportDialog that fires during the move —
		// either from the immediate transit below or from WALKING onto the entrance
		// later in the pack — is answered with the pathfinder's exit.
		for(auto it = path.nodes.rbegin(); it != path.nodes.rend(); ++it)
		{
			if(it->coord != hero->visitablePos() && it->isTeleportAction())
			{
				std::lock_guard<std::mutex> lock(teleportExitMx);
				plannedTeleportExit = it->coord;
				break;
			}
		}
		if(debugMove)
		{
			std::string nodesDump;
			int dumped = 0;
			for(auto it = path.nodes.rbegin(); it != path.nodes.rend() && dumped < 10; ++it, ++dumped)
				nodesDump += it->coord.toString() + " a" + std::to_string(static_cast<int>(it->action))
					+ " t" + std::to_string(static_cast<int>(it->turns)) + " | ";
			std::ofstream dbg("/tmp/arena_move_debug.log", std::ios::app);
			dbg << "move hero=" << hero->getNameTranslated()
				<< " at=" << hero->visitablePos().toString()
				<< " dest=" << destination.toString()
				<< " stopBeforeCombat=" << stopBeforeCombat
				<< " path: " << nodesDump << "\n";
		}
		// Mirror the client's HeroMovementController batch construction. Nodes are
		// stored destination-first, so iterate in reverse (start -> destination).
		// CRUCIAL: path coords are visitable positions; moveHero expects the hero's
		// own tile coords, hence convertFromVisitablePos (omitting this was why the
		// server kept rejecting moves as "destination tile is blocked").
		for(auto it = path.nodes.rbegin(); it != path.nodes.rend(); ++it)
		{
			const CGPathNode & node = *it;
			if(node.coord == hero->visitablePos())
				continue; // start node = current hero position
			if(node.isTeleportAction())
			{
				// The path's next step is a teleport transit (hero stands ON the
				// monolith / subterranean gate). Breaking here with zero tiles queued
				// used to be a SILENT no-op: moveHero was never called, the hero
				// spent no movement, yet the decision was logged as executed — heroes
				// stranded on portal tiles forever. Mirror the client
				// (HeroMovementController::sendMovementRequest): a move request onto
				// the hero's own anchor tile re-visits the teleporter and carries the
				// hero through; the TeleportDialog query is answered via the bridge.
				// Movement resumes from the exit on the next decision / auto-advance.
				if(tiles.empty() && node.turns == 0)
				{
					const bool exitStartsCombat = node.action == EPathNodeAction::TELEPORT_BATTLE
						|| cb->guardingCreaturePosition(node.coord) != int3(-1, -1, -1);
					if(stopBeforeCombat && exitStartsCombat)
					{
						if(blockedByGuard != nullptr)
							*blockedByGuard = true;
					}
					else
					{
						cb->moveHero(hero, hero->pos, false, EPathfindingLayer::AUTO);
						if(node.coord == destination)
							reachedDestination = true;
					}
				}
				break; // tiles already queued: walk them and stop before the portal
			}
			if(node.turns != 0)
				break; // out of movement points this turn
			// "Does stepping onto this node start a fight?" — use the engine's own
			// pathfinder classification, which marks BOTH wandering-monster guards AND
			// object-embedded battles (enemy hero/town, creature bank, garrison, border
			// guard, ...) as a BATTLE action. The previous guardingCreaturePosition()
			// check only caught wandering-monster ZoC, so a path that stepped onto a
			// battle-triggering *object* slipped through and started a battle inside
			// auto-advance — the reproducible 0%-CPU deadlock (auto-advance has no
			// waitForBattles handshake). guardingCreaturePosition() is kept as a
			// belt-and-suspenders fallback.
			const bool startsCombat =
				node.action == EPathNodeAction::BATTLE
				|| node.action == EPathNodeAction::TELEPORT_BATTLE
				|| cb->guardingCreaturePosition(node.coord) != int3(-1, -1, -1);
			if(stopBeforeCombat && startsCombat)
			{
				// Auto-advance must never start a battle: stop BEFORE the combat tile
				// (do not push it) and let the model decide whether to engage. Battles
				// are only safe via the model-driven action loop.
				if(blockedByGuard != nullptr)
					*blockedByGuard = true;
				break;
			}
			tiles.push_back(hero->convertFromVisitablePos(node.coord));
			const bool atDestination = (node.coord == destination);
			if(atDestination)
				reachedDestination = true;
			if(startsCombat)
				break; // stepping here starts a battle (only when !stopBeforeCombat)
			if(!cb->getVisitableObjs(node.coord).empty())
				break; // reached a visitable object / garrison / event
		}
	}

	if(debugMove)
	{
		std::ofstream dbg("/tmp/arena_move_debug.log", std::ios::app);
		dbg << "  -> tiles=" << tiles.size()
			<< " reached=" << reachedDestination
			<< (path.nodes.empty() ? " NO_PATH" : "") << "\n";
	}
	if(!tiles.empty())
		cb->moveHero(hero, tiles, false, EPathfindingLayer::AUTO);
	return reachedDestination;
}

const CGObjectInstance * CArenaAI::unownedObjectAt(const int3 & pos) const
{
	for(const auto * obj : cb->getVisitableObjs(pos, false))
	{
		if(obj == nullptr)
			continue;
		if(obj->tempOwner == playerID)
			continue; // already ours
		return obj;
	}
	return nullptr;
}

void CArenaAI::advanceTravelGoals()
{
	// Kill-switch: ARENA_DISABLE_AUTO_ADVANCE=1 falls back to pure model-driven
	// movement (prune-owned options still apply) without rebuilding the plugin.
	if(const char * disable = std::getenv("ARENA_DISABLE_AUTO_ADVANCE"))
		if(disable[0] == '1')
			return;

	for(auto it = heroTravelGoals.begin(); it != heroTravelGoals.end();)
	{
		if(aborting)
			break;
		const CGHeroInstance * hero = findHeroById(it->first);
		const int3 goal = it->second;
		// Drop the goal if the hero is gone or the target is no longer a worthwhile
		// (unowned) object -- e.g. we already captured it or it was consumed.
		if(hero == nullptr || unownedObjectAt(goal) == nullptr)
		{
			it = heroTravelGoals.erase(it);
			continue;
		}
		// Only auto-advance while a path to the goal still exists.
		CPathsInfo heroPaths(cb->getMapSize(), hero);
		auto pathConfig = std::make_shared<SingleHeroPathfinderConfig>(heroPaths, *cb, hero);
		cb->calculatePaths(pathConfig);
		const CGPathNode * node = heroPaths.getPathInfo(goal);
		if(node == nullptr || !node->reachable())
		{
			it = heroTravelGoals.erase(it);
			continue;
		}
		// Combat-free advance: travel toward the goal capturing only unguarded
		// objects. If a guard blocks the way, hand the engagement decision back to
		// the model (battles are only safe via the model-driven action loop, which
		// runs inside the per-action time budget) and drop the goal.
		bool blockedByGuard = false;
		const bool reached = executeHeroMoveTo(hero, goal, /*stopBeforeCombat=*/true, &blockedByGuard);
		// Defense-in-depth: stepBeforeCombat above should keep auto-advance battle-free,
		// but if a battle ever slips through (e.g. a path node the pathfinder did not
		// classify as BATTLE), drain it with the same release-lock/wait handshake the
		// per-action loop uses — never leave a CBattleQuery open while we hold the
		// shared game-state lock (that is the deadlock this guards against).
		waitForBattles();
		if(reached || blockedByGuard || unownedObjectAt(goal) == nullptr)
			it = heroTravelGoals.erase(it);
		else
			++it;
	}
}

void CArenaAI::autoCollectGarrisons()
{
	// Kill-switch (mirrors auto-advance), in case it ever needs disabling without a rebuild.
	if(const char * disable = std::getenv("ARENA_DISABLE_AUTO_COLLECT"))
		if(disable[0] == '1')
			return;

	for(const auto * town : cb->getTownsInfo(true))
	{
		if(town == nullptr)
			continue;
		const CGHeroInstance * hero = town->getVisitingHero();
		if(hero == nullptr)
			continue; // no hero standing in the town to receive the garrison
		// Snapshot the occupied garrison slots first; each move realizes (waitTillRealize)
		// before the next, so re-reading getCreature(slot) reflects the post-move state.
		std::vector<SlotID> slots;
		for(const auto & s : town->Slots())
			slots.push_back(s.first);
		for(const SlotID slot : slots)
		{
			if(aborting)
				break;
			const CCreature * cre = town->getCreature(slot);
			if(cre == nullptr)
				continue; // slot already emptied
			const SlotID dst = hero->getSlotFor(cre);
			if(!dst.validSlot())
				break; // hero army full (7 distinct stacks) — leave the rest in town
			cb->mergeOrSwapStacks(town, hero, slot, dst); // full stack, no leave-one
		}
	}
}

void CArenaAI::answerQuery(QueryID queryID, int choice)
{
	if(queryID.getNum() < 0)
		return;

	// Query replies arrive on the network thread (post-battle level-up, mid-move
	// blocking dialogs, ...). With waitTillRealize=true a blocking reply would wedge
	// the network thread waiting on a PackageApplied only it can deliver, so send the
	// reply without waiting (mirrors CBattleAI's temporary waitTillRealize toggle).
	const bool prev = cb->waitTillRealize;
	cb->waitTillRealize = false;
	cb->selectionMade(choice, queryID);
	cb->waitTillRealize = prev;
}

void CArenaAI::battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side, bool replayAllowed)
{
	{
		std::lock_guard<std::mutex> lock(battleMx);
		++battlesInProgress;
	}
	battleCv.notify_all();
	// Delegate to the inherited adventure-AI battle handling: spins up the configured
	// battle AI (BattleAI) and plays the fight out on the network thread.
	CAdventureAI::battleStart(battleID, army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void CArenaAI::battleEnd(const BattleID & battleID, const BattleResult * br, QueryID queryID)
{
	// Tear down the battle AI first (this restores cb->waitTillRealize to true).
	CAdventureAI::battleEnd(battleID, br, queryID);
	{
		std::lock_guard<std::mutex> lock(battleMx);
		if(battlesInProgress > 0)
			--battlesInProgress;
	}
	battleCv.notify_all();
}

void CArenaAI::gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult)
{
	(void)victoryLossCheckResult;
	// When our own game resolves, release the worker so it stops issuing actions
	// (and skips a post-game endTurn that would never be realized).
	if(player == playerID)
	{
		aborting = true;
		battleCv.notify_all();
	}
}

void CArenaAI::heroGotLevel(const CGHeroInstance * hero, PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID)
{
	(void)hero;
	(void)pskill;
	if(skills.empty())
	{
		answerQuery(queryID, 0);
		return;
	}
	std::vector<int> options;
	for(int idx = 0; idx < static_cast<int>(skills.size()); ++idx)
		options.push_back(idx);
	const int choice = chooseSelectionViaBridge(queryID, "hero_level_up", options, 0);
	answerQuery(queryID, choice);
}

void CArenaAI::commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID)
{
	(void)commander;
	if(skills.empty())
	{
		answerQuery(queryID, 0);
		return;
	}
	std::vector<int> options;
	for(int idx = 0; idx < static_cast<int>(skills.size()); ++idx)
		options.push_back(idx);
	const int choice = chooseSelectionViaBridge(queryID, "commander_level_up", options, 0);
	answerQuery(queryID, choice);
}

void CArenaAI::showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel, bool safeToAutoaccept)
{
	(void)soundID;
	(void)safeToAutoaccept;

	std::vector<int> options;
	if(selection)
	{
		if(cancel)
			options.push_back(0);
		for(int idx = 0; idx < static_cast<int>(components.size()); ++idx)
			options.push_back(idx + 1);
	}
	else if(cancel)
	{
		// Fix 6: a yes/no dialog (selection == false, cancel == true) — e.g. a border guard
		// "do you wish to pass?", a creature "fight/flee/join?", a mine/dwelling flag prompt.
		// The engine interprets answer != 0 as "yes"/accept (CGBorderGuard::blockingDialogAnswered
		// removeObject if(answer); CGCreature routes it to join/flee). We previously offered only
		// {0} = permanent "no", so the bot could never pass a border guard even holding the key.
		// Now emit BOTH so the policy can accept — the default remains 0 (lowest option id) so
		// entrants that don't override query handling keep today's decline-everything behavior.
		options.push_back(0);
		options.push_back(1);
	}
	else
	{
		// OK-only informational dialog: a single acknowledgement.
		options.push_back(0);
	}

	const int defaultSelection = cancel ? 0 : (selection ? 1 : 0);
	const int choice = chooseSelectionViaBridge(askID, "blocking_dialog", options, defaultSelection, text);
	answerQuery(askID, choice);
}

void CArenaAI::showTeleportDialog(const CGHeroInstance * hero, TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID)
{
	(void)hero;
	(void)channel;

	if(impassable || exits.empty())
	{
		answerQuery(askID, -1);
		return;
	}

	// Prefer the exit the current move's path was computed through (mirrors the
	// human client / NKAI). A blind first-option answer sent heroes out of the
	// WRONG monolith of a multi-exit channel: the pathfinder re-planned through
	// the teleporter, the dialog undid it again — a permanent cross-pocket
	// ping-pong. -1 = let the server pick when we have no planned exit (manual
	// model-driven entry with no teleport node in the path).
	int3 wantedExit(-1, -1, -1);
	{
		std::lock_guard<std::mutex> lock(teleportExitMx);
		wantedExit = plannedTeleportExit;
		plannedTeleportExit = int3(-1, -1, -1);
	}
	if(wantedExit.isValid())
	{
		for(int idx = 0; idx < static_cast<int>(exits.size()); ++idx)
		{
			if(exits[idx].second == wantedExit)
			{
				answerQuery(askID, idx);
				return;
			}
		}
	}
	answerQuery(askID, -1);
}

void CArenaAI::showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID, const MetaString & customTitle)
{
	(void)up;
	(void)down;
	(void)removableUnits;
	(void)customTitle;
	// Answer immediately: the only selectable option is 0, and a bridge
	// round-trip here opens a race window — this query can arrive after the
	// triggering move already reported complete, so the turn loop issues its
	// next action against the pending query and the headless session tears
	// down ("Player X has to answer queries"), parking the client forever.
	answerQuery(queryID, 0);
}

void CArenaAI::showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects)
{
	(void)icon;
	(void)title;
	(void)description;

	std::vector<int> options;
	if(objects.empty())
		options.push_back(0);
	else
		for(int idx = 0; idx < static_cast<int>(objects.size()); ++idx)
			options.push_back(idx);

	const int choice = chooseSelectionViaBridge(askID, "map_object_select", options, 0);
	answerQuery(askID, choice);
}

void CArenaAI::showMarketWindow(const IMarket * market, const CGHeroInstance * visitor, QueryID queryID)
{
	(void)market;
	(void)visitor;
	answerQuery(queryID, 0);
}

std::optional<BattleAction> CArenaAI::makeSurrenderRetreatDecision(const BattleID & battleID, const BattleStateInfoForRetreat & battleState)
{
	(void)battleID;
	(void)battleState;
	return std::nullopt;
}
