/*
 * QuestTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "QuestTest.h"

#include "../../lib/CPlayerState.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGObjectInstance.h"

void QuestTest::startWithMap(TinyH3M::TinyH3MBuilder builder)
{
	// Materialise the scenario, hand the bytes to a fresh map service. The
	// gameState init pipeline calls back into mapLoaded() once the CMap is
	// constructed.
	auto bytes = builder.build();
	mapService = std::make_unique<MapServiceTinyH3M>(std::move(bytes), this);

	StartInfo si;
	si.mapname    = "tiny";   // arbitrary; the service ignores the ResourcePath.
	si.difficulty = 0;
	si.mode       = EStartMode::NEW_GAME;

	auto header = mapService->loadMapHeader(ResourcePath(si.mapname));
	ASSERT_NE(header.get(), nullptr) << "TinyH3M scenario header failed to load";

	for(int i = 0; i < static_cast<int>(header->players.size()); ++i)
	{
		const PlayerInfo & pinfo = header->players[i];
		if(!(pinfo.canHumanPlay || pinfo.canComputerPlay))
			continue;

		PlayerSettings & pset = si.playerInfos[PlayerColor(i)];
		pset.color  = PlayerColor(i);
		pset.connectedPlayerIDs.insert(static_cast<PlayerConnectionID>(i));
		pset.name   = "Player";
		pset.castle = pinfo.defaultCastle();
		pset.hero   = pinfo.defaultHero();
	}

	GameRandomizer randomizer(*gameState);
	Load::ProgressAccumulator progressTracker;
	gameState->init(mapService.get(), &si, randomizer, progressTracker, false);

	ASSERT_NE(map, nullptr) << "gameState init did not populate the CMap";

	// Wire the event mock's object lookup. The mock needs an IGameInfoCallback
	// for takeCreatures and any future override that resolves ObjectInstanceIDs
	// (CGameState is itself an IGameInfoCallback subclass).
	gameEventCallback->setGameInfoCallback(gameState.get());
}

CGObjectInstance * QuestTest::findObjectAt(const int3 & pos) const
{
	for(const auto & obj : map->objects)
	{
		if(obj && obj->anchorPos() == pos)
			return obj.get();
	}
	return nullptr;
}

CGHeroInstance * QuestTest::findHeroAt(const int3 & pos) const
{
	for(const auto & obj : map->objects)
	{
		auto * hero = dynamic_cast<CGHeroInstance *>(obj.get());
		if(hero && hero->anchorPos() == pos)
			return hero;
	}
	return nullptr;
}

CGHeroInstance * QuestTest::findHeroByOwner(PlayerColor owner) const
{
	for(const auto & obj : map->objects)
	{
		auto * hero = dynamic_cast<CGHeroInstance *>(obj.get());
		if(hero && hero->getOwner() == owner)
			return hero;
	}
	return nullptr;
}

void QuestTest::grantResources(PlayerColor player, GameResID which, int amount)
{
	auto it = gameState->players.find(player);
	ASSERT_NE(it, gameState->players.end()) << "Player " << player.getNum() << " not initialised";
	it->second.resources[which] += amount;
}

void QuestTest::markObjectDestroyed(PlayerColor player, ObjectInstanceID target)
{
	auto it = gameState->players.find(player);
	ASSERT_NE(it, gameState->players.end()) << "Player " << player.getNum() << " not initialised";
	it->second.destroyedObjects.insert(target);
}

void QuestTest::visit(CGHeroInstance * hero, CGObjectInstance * obj)
{
	ASSERT_NE(hero, nullptr);
	ASSERT_NE(obj, nullptr);
	obj->onHeroVisit(*gameEventCallback, hero);
}

void QuestTest::answerDialog(CGHeroInstance * hero, int32_t answer)
{
	// Consume the most recent BlockingDialog from the mock's queue and drive
	// the caller's blockingDialogAnswered with the chosen answer. Pop-from-back
	// because nested visits push dialogs in LIFO order and the answer should
	// resolve the innermost prompt first.
	auto & queue = gameEventCallback->blockingDialogs;
	ASSERT_FALSE(queue.empty())
		<< "answerDialog called with no pending BlockingDialog — visit must enqueue one first";
	auto captured = queue.back();
	queue.pop_back();

	ASSERT_NE(captured.caller, nullptr)
		<< "captured BlockingDialog has no caller; blockingDialogAnswered would have nothing to dispatch on";
	captured.caller->blockingDialogAnswered(*gameEventCallback, hero, answer);
}

void QuestTest::overrideSettingBeforeInit(EGameSettings option, bool value)
{
	pendingOverrides.push_back({option, value});
}

void QuestTest::advanceDays(int days)
{
	// Lightweight: bump the day counter directly. CQuest::lastDay is compared
	// against gameState->day, which is enough to drive the timeout path. Tests
	// requiring per-turn side effects (creature growth, hero replenishment)
	// will need a more involved nextDay() helper later.
	ASSERT_GE(days, 0);
	gameState->day += static_cast<ui32>(days);
}
