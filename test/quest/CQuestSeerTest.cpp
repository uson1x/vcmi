/*
 * CQuestSeerTest.cpp, part of VCMI engine
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

#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CQuest.h"

// Phase 2.2 — seer hut runtime tests. Just one for now so the whole stack
// (scenario factory -> startWithMap -> CQuest::checkQuest -> Limiter
// evaluation against a real CGHeroInstance) is proven to wire up before the
// rest of the seer-hut suite lands.

using namespace QuestScenarios;

class QuestSeerTest : public QuestTest {};

TEST_F(QuestSeerTest, Level_passesAtThreshold)
{
	// seerLevel() places a hero with 10000 XP (roughly level 5) and two seer
	// huts: an "easy" one demanding heroLevel >= 3 and a "hard" one demanding
	// heroLevel >= 10. CQuest::checkQuest -> Limiter::heroAllowed compares
	// against the hero's derived level field (populated by levelUpAutomatically
	// during gameState init), so the easy quest should pass and the hard one
	// should fail.
	auto s = seerLevel();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * hero = findHeroAt(s.heroPos);
	ASSERT_NE(hero, nullptr);
	EXPECT_GE(hero->level, 3u) << "scenario assumes hero is at least level 3 from its starting XP";

	const auto * easy = dynamic_cast<const CGSeerHut *>(findObjectAt(s.questPos));
	const auto * hard = dynamic_cast<const CGSeerHut *>(findObjectAt(s.questPos2));
	ASSERT_NE(easy, nullptr);
	ASSERT_NE(hard, nullptr);

	EXPECT_TRUE (easy->getQuest().checkQuest(hero))
		<< "easy seer (level >= 3) should accept a level-" << hero->level << " hero";
	EXPECT_FALSE(hard->getQuest().checkQuest(hero))
		<< "hard seer (level >= 10) should reject a level-" << hero->level << " hero";
}
