/*
 * CQuestGuardTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "QuestScenarios.h"
#include "QuestTest.h"

#include "../../lib/CPlayerState.h"
#include "../../lib/gameState/CGameState.h"
#include "../../lib/mapObjects/CGObjectInstance.h"
#include "../../lib/mapObjects/CQuest.h"

// Phase 2.3 — quest-guard runtime tests. Visit-driven flows
// (SatisfiedQuest_removesObjectOnAcceptance, RepeatVisit_failedRequirements,
// BringResources_takesResources) need the dialog-answer + take-resources +
// removeObject packet wiring from a later 0.4 pass.

using namespace QuestScenarios;

class QuestGuardTest : public QuestTest {};

TEST_F(QuestGuardTest, PassableForColor_alwaysFalse)
{
	// CGQuestGuard::passableFor(color) returns getQuest().isCompleted. A
	// freshly loaded quest guard has isCompleted=false irrespective of which
	// colour asks, so adventure-map pathfinding never lets a hero walk
	// through it directly.
	auto s = questGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * guard = findObjectAt(s.questPos);
	ASSERT_NE(guard, nullptr);
	for(int c = 0; c < 8; ++c)
		EXPECT_FALSE(guard->passableFor(PlayerColor(c)))
			<< "quest guard should block colour " << c << " before completion";
}

TEST_F(QuestGuardTest, BlockVisitIsTrue_cannotBeTriggeredFromOnTop)
{
	// Quest Guard sets blockVisit=true in its initObj. The adventure map
	// pathfinder distinguishes "blocking visitable" tiles from "visitable"
	// tiles so a hero standing on the guard cannot trigger it implicitly.
	// We only assert the flag here — the actual pathfinder integration is
	// covered by the engine's own tests.
	auto s = questGuardBlockVisit();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * guard = findObjectAt(s.questPos);
	ASSERT_NE(guard, nullptr);
	EXPECT_TRUE(guard->isBlockedVisitable());
}

TEST_F(QuestGuardTest, BringResources_takesResources)
{
	// Scenario asks for 1000 wood; test grants 1500 post-load on top of the
	// starting bonus (~30 wood for a knight on difficulty 0 — not pinned down,
	// so the assertion measures the delta, not an absolute). Quest-guard
	// subID=0 sets reward.removeObject=true in its init, so the guard object
	// is gone after acceptance — see SatisfiedQuest_removesObjectOnAcceptance.
	auto s = questGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	grantResources(PlayerColor(0), GameResID(GameResID::WOOD), 1500);
	const auto woodBefore = gameState->players.at(PlayerColor(0)).resources[GameResID::WOOD];
	ASSERT_GE(woodBefore, 1000) << "scenario must leave hero with at least the demanded amount";

	auto * hero  = findHeroAt(s.heroPos);
	auto * guard = findObjectAt(s.questPos);
	ASSERT_NE(hero,  nullptr);
	ASSERT_NE(guard, nullptr);

	visit(hero, guard);
	ASSERT_EQ(gameEventCallback->blockingDialogs.size(), 1u)
		<< "first visit with a satisfied limiter should prompt the player";

	answerDialog(hero, /*yes*/ 1);

	const auto woodAfter = gameState->players.at(PlayerColor(0)).resources[GameResID::WOOD];
	EXPECT_EQ(woodBefore - woodAfter, 1000)
		<< "quest should deduct exactly the demanded 1000 wood";
}

TEST_F(QuestGuardTest, SatisfiedQuest_removesObjectOnAcceptance)
{
	// CGQuestGuard::init sets reward.removeObject=true when subID==0. The
	// TinyH3MBuilder emits subID=0 for every quest guard, so accepting a
	// satisfied mission removes the guard from the map.
	auto s = questGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	grantResources(PlayerColor(0), GameResID(GameResID::WOOD), 1500);

	auto * hero  = findHeroAt(s.heroPos);
	auto * guard = findObjectAt(s.questPos);
	ASSERT_NE(hero,  nullptr);
	ASSERT_NE(guard, nullptr);

	visit(hero, guard);
	ASSERT_EQ(gameEventCallback->blockingDialogs.size(), 1u);
	answerDialog(hero, /*yes*/ 1);

	EXPECT_EQ(findObjectAt(s.questPos), nullptr)
		<< "quest guard with subID=0 should be removed from the map after acceptance";
}

TEST_F(QuestGuardTest, RepeatVisit_failedRequirements_showsNextVisitText)
{
	// First visit on an unsatisfied limiter emits AddQuest + first-visit
	// InfoWindow. Second visit while still unsatisfied emits the
	// next-visit InfoWindow only (no second AddQuest).
	auto s = questGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	// Deliberately do not grant resources — limiter fails.

	auto * hero  = findHeroAt(s.heroPos);
	auto * guard = findObjectAt(s.questPos);
	ASSERT_NE(hero,  nullptr);
	ASSERT_NE(guard, nullptr);

	visit(hero, guard);
	const size_t firstAddQuests = gameEventCallback->addedQuests.size();
	const size_t firstWindows   = gameEventCallback->infoWindows.size();
	EXPECT_EQ(firstAddQuests, 1u);
	EXPECT_EQ(firstWindows,   1u);

	visit(hero, guard);
	EXPECT_EQ(gameEventCallback->addedQuests.size(), firstAddQuests)
		<< "second visit must not re-emit AddQuest";
	EXPECT_GT(gameEventCallback->infoWindows.size(), firstWindows)
		<< "second visit should produce its own next-visit info dialog";
	EXPECT_TRUE(gameEventCallback->blockingDialogs.empty())
		<< "unsatisfied limiter must not prompt for completion";
}
