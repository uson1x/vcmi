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

void QuestTest::visit(CGHeroInstance * hero, CGObjectInstance * obj)
{
	ASSERT_NE(hero, nullptr);
	ASSERT_NE(obj, nullptr);
	obj->onHeroVisit(*gameEventCallback, hero);
}

void QuestTest::answerDialog(CGHeroInstance * /*hero*/, int32_t /*answer*/)
{
	// Wired once GameEventCallbackMock starts queueing blocking dialogs (0.4).
	// Left as a stub so test files can refer to it without compilation error;
	// the first test that needs it will drive the queue consumption here.
	FAIL() << "answerDialog: blocking-dialog queue is not wired yet (see Phase 0.4)";
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
