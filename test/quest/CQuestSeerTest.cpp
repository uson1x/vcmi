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

#include "../../lib/mapObjects/CGCreature.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/mapObjects/MiscObjects.h"

// Phase 2.2 — seer-hut runtime tests. Each test exercises CQuest::checkQuest
// against the loaded state without driving onHeroVisit / netpacks (those
// require packet-forwarder wiring beyond what Phase 0.4 currently has).
// Visit-flow tests (FirstVisit_emitsAddQuest, RepeatVisit_*, GrantsReward,
// CompletedOneShot, HoverText, Timeout) land alongside the matching 0.4
// forwarder extension PRs.

using namespace QuestScenarios;

class QuestSeerTest : public QuestTest
{
protected:
	const CGSeerHut * seerAt(const int3 & pos)
	{
		auto * obj = findObjectAt(pos);
		return obj ? dynamic_cast<const CGSeerHut *>(obj) : nullptr;
	}
};

TEST_F(QuestSeerTest, Level_passesAtThreshold)
{
	// seerLevel() places a hero with 10000 XP (≈ level 5) and two seer huts:
	// easy (level >= 3) and hard (level >= 10). Limiter::heroAllowed compares
	// against the hero's derived level field (populated by
	// levelUpAutomatically during gameState init).
	auto s = seerLevel();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * hero = findHeroAt(s.heroPos);
	ASSERT_NE(hero, nullptr);
	EXPECT_GE(hero->level, 3u) << "scenario assumes hero is at least level 3 from its starting XP";

	const auto * easy = seerAt(s.questPos);
	const auto * hard = seerAt(s.questPos2);
	ASSERT_NE(easy, nullptr);
	ASSERT_NE(hard, nullptr);

	EXPECT_TRUE (easy->getQuest().checkQuest(hero));
	EXPECT_FALSE(hard->getQuest().checkQuest(hero));
}

TEST_F(QuestSeerTest, PrimarySkill_passesAtThreshold)
{
	// seerPrimarySkill() places a hero with attack=5 plus easy (>=3) and
	// hard (>=10) primary-skill seers. The limiter compares each requested
	// primary against the hero's pushPrimSkill-populated stats.
	auto s = seerPrimarySkill();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * hero = findHeroAt(s.heroPos);
	ASSERT_NE(hero, nullptr);

	const auto * easy = seerAt(s.questPos);
	const auto * hard = seerAt(s.questPos2);
	ASSERT_NE(easy, nullptr);
	ASSERT_NE(hard, nullptr);

	EXPECT_TRUE (easy->getQuest().checkQuest(hero));
	EXPECT_FALSE(hard->getQuest().checkQuest(hero));
}

TEST_F(QuestSeerTest, BringHero_passesWhenHeroPresent)
{
	// Mission asks for HeroTypeID(kHeroTyris); scenario places both Christian
	// and Tyris under the red player. Limiter::heroAllowed for a HERO mission
	// just checks that the visiting hero is in the `heroes` whitelist.
	auto s = seerHero();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * christian = findHeroAt(s.heroPos);
	const auto * tyris     = findHeroAt(s.secondHeroPos);
	ASSERT_NE(christian, nullptr);
	ASSERT_NE(tyris,     nullptr);

	const auto * seer = seerAt(s.questPos);
	ASSERT_NE(seer, nullptr);

	EXPECT_FALSE(seer->getQuest().checkQuest(christian)) << "Christian is not the target hero";
	EXPECT_TRUE (seer->getQuest().checkQuest(tyris))     << "Tyris is the target hero";
}

TEST_F(QuestSeerTest, BringPlayer_passesForCorrectColor)
{
	// Mission asks for PlayerColor(1); scenario places one hero per colour.
	// Limiter::heroAllowed for a PLAYER mission whitelists by hero owner.
	auto s = seerPlayer();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * red  = findHeroByOwner(PlayerColor(0));
	const auto * blue = findHeroByOwner(PlayerColor(1));
	ASSERT_NE(red,  nullptr);
	ASSERT_NE(blue, nullptr);

	const auto * seer = seerAt(s.questPos);
	ASSERT_NE(seer, nullptr);

	EXPECT_FALSE(seer->getQuest().checkQuest(red))  << "red is not the target colour";
	EXPECT_TRUE (seer->getQuest().checkQuest(blue)) << "blue is the target colour";
}

TEST_F(QuestSeerTest, KillCreature_satisfiedAfterCreatureRemoved)
{
	// CQuest::checkQuest fails for a kill-quest until the target's id is in
	// PlayerState::destroyedObjects for the visiting hero's owner. We
	// short-circuit the actual battle by mutating destroyedObjects directly.
	auto s = seerKillCreature();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * hero    = findHeroAt(s.heroPos);
	const auto * monster = dynamic_cast<const CGCreature *>(findObjectAt(s.secondHeroPos));
	const auto * seer    = seerAt(s.questPos);
	ASSERT_NE(hero,    nullptr);
	ASSERT_NE(monster, nullptr);
	ASSERT_NE(seer,    nullptr);

	EXPECT_FALSE(seer->getQuest().checkQuest(hero))
		<< "target still alive, kill-quest should not be satisfied";

	markObjectDestroyed(PlayerColor(0), monster->id);

	EXPECT_TRUE(seer->getQuest().checkQuest(hero))
		<< "target marked destroyed, kill-quest should be satisfied";
}

TEST_F(QuestSeerTest, KillHero_satisfiedAfterHeroDefeated)
{
	auto s = seerKillHero();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));

	const auto * visitor = findHeroAt(s.heroPos);
	const auto * target  = findHeroAt(s.secondHeroPos);
	const auto * seer    = seerAt(s.questPos);
	ASSERT_NE(visitor, nullptr);
	ASSERT_NE(target,  nullptr);
	ASSERT_NE(seer,    nullptr);

	EXPECT_FALSE(seer->getQuest().checkQuest(visitor));

	markObjectDestroyed(PlayerColor(0), target->id);

	EXPECT_TRUE(seer->getQuest().checkQuest(visitor));
}
