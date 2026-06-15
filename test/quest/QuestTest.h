/*
 * QuestTest.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "StdInc.h"

#include "../mock/TinyH3MBuilder.h"
#include "../mock/mock_MapServiceTinyH3M.h"
#include "../mock/mock_Services.h"
#include "../mock/mock_IGameEventCallback.h"

#include "../../lib/CRandomGenerator.h"
#include "../../lib/IGameSettings.h"
#include "../../lib/StartInfo.h"
#include "../../lib/callback/GameRandomizer.h"
#include "../../lib/filesystem/ResourcePath.h"
#include "../../lib/gameState/CGameState.h"
#include "../../lib/mapping/CMap.h"
#include "../../lib/mapping/CMapHeader.h"

#include <vcmi/ServerCallback.h>

class CGObjectInstance;
class CGHeroInstance;
class CGSeerHut;
class CGQuestGuard;

/// Base fixture for quest behaviour tests. Construct a scenario inline with
/// TinyH3MBuilder, hand it to startWithMap(), then drive interactions through
/// the helpers below. Mirrors the structure of GameStateTest but swaps the
/// JSON-archive MapServiceMock for the in-memory MapServiceTinyH3M.
///
/// Object lookup is position- or type-based — the builder does not set the
/// instance-name index, so tests reference what they placed by coordinate.
class QuestTest : public ::testing::Test, public ServerCallback, public MapListener
{
public:
	QuestTest()
		: gameEventCallback(std::make_shared<GameEventCallbackMock>(this))
	{
	}

	void SetUp() override
	{
		gameState = std::make_shared<CGameState>();
		gameState->preInit(&services);
	}

	void TearDown() override
	{
		gameState.reset();
		mapService.reset();
		map = nullptr;
	}

	// ---- ServerCallback overrides ---------------------------------------

	bool describeChanges() const override { return true; }
	void apply(CPackForClient & pack) override { gameState->apply(pack); }
	void complain(const std::string & problem) override
	{
		FAIL() << "Server-side assertion: " << problem;
	}
	vstd::RNG * getRNG() override { return &randomGenerator; }

	// Battle-specific overrides are no-ops for quest tests.
	void apply(BattleLogMessage &) override {}
	void apply(BattleStackMoved &) override {}
	void apply(BattleUnitsChanged &) override {}
	void apply(SetStackEffect &) override {}
	void apply(StacksInjured &) override {}
	void apply(BattleObstaclesChanged &) override {}
	void apply(CatapultAttack &) override {}

	// ---- MapListener override -------------------------------------------

	void mapLoaded(CMap * loadedMap) override
	{
		EXPECT_EQ(this->map, nullptr);
		this->map = loadedMap;
		// Apply pre-init setting overrides while CMap exists but before
		// gameState->init walks the object list and calls initObj/init on
		// each one (where some settings get baked into per-object state).
		for(const auto & ov : pendingOverrides)
		{
			JsonNode node;
			node.Bool() = ov.value;
			loadedMap->overrideGameSetting(ov.option, node);
		}
	}

	// ---- public test API ------------------------------------------------

	/// Build the scenario, register the bytes with a fresh MapServiceTinyH3M,
	/// and run the gameState init pipeline. After this returns the `map`
	/// pointer and `gameState` are ready for assertions.
	void startWithMap(TinyH3M::TinyH3MBuilder builder);

	// Position-based lookup. Returns the first object whose anchor matches.
	CGObjectInstance * findObjectAt(const int3 & pos) const;
	CGHeroInstance   * findHeroAt(const int3 & pos) const;
	CGHeroInstance   * findHeroByOwner(PlayerColor owner) const;

	template<class T>
	T * findFirst() const
	{
		for(const auto & obj : map->objects)
		{
			if(auto * casted = dynamic_cast<T *>(obj.get()))
				return casted;
		}
		return nullptr;
	}

	template<class T>
	std::vector<T *> findAll() const
	{
		std::vector<T *> out;
		for(const auto & obj : map->objects)
		{
			if(auto * casted = dynamic_cast<T *>(obj.get()))
				out.push_back(casted);
		}
		return out;
	}

	// Grant resources to a player after load. The builder does not set
	// starting resources, so any "hero brings 1000 gold" test seeds them
	// here. Implemented by direct PlayerState mutation — bypasses the
	// netpack pipeline because no one is observing pre-test state changes.
	void grantResources(PlayerColor player, GameResID which, int amount);

	// Mark `target` as destroyed by `player`. Mutates
	// PlayerState::destroyedObjects directly — the slot CQuest::checkQuest
	// consults for kill-quest target resolution. Avoids simulating an entire
	// adventure-map battle just to flip the destroyed flag.
	void markObjectDestroyed(PlayerColor player, ObjectInstanceID target);

	// Trigger onHeroVisit directly. Pathfinding is bypassed; for tests that
	// must exercise blockVisit / passableFor, drive moveHero through the
	// netpack pipeline instead.
	void visit(CGHeroInstance * hero, CGObjectInstance * obj);

	// Drive the most recent BlockingDialog. Throws if there is none queued
	// (consumeBlockingDialog returns false).
	void answerDialog(CGHeroInstance * hero, int32_t answer);

	// Advance the in-game day counter by `days`. Used by lastDay / reach-date
	// tests. The implementation calls gameState->onTurn() that many times.
	void advanceDays(int days);

	// Schedule an EGameSettings override to apply before any object's init()
	// runs. Some settings (e.g. MAP_OBJECTS_H3_BUG_QUEST_TAKES_ENTIRE_ARMY)
	// are baked into per-object state during init — toggling them post-init
	// changes the engine's runtime read of the setting but not the stale
	// mission.hasExtraCreatures field on objects that were already inited.
	// Call this BEFORE startWithMap.
	void overrideSettingBeforeInit(EGameSettings option, bool value);

protected:
	struct PendingOverride
	{
		EGameSettings option;
		bool          value;
	};
	std::vector<PendingOverride> pendingOverrides;

	std::shared_ptr<CGameState>            gameState;
	std::shared_ptr<GameEventCallbackMock> gameEventCallback;
	std::unique_ptr<MapServiceTinyH3M>     mapService;
	ServicesMock                           services;
	CMap *                                 map = nullptr;
	CRandomGenerator                       randomGenerator;
};
