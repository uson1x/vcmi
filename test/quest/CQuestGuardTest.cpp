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
