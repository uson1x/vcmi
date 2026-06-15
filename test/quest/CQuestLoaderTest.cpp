/*
 * CQuestLoaderTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "QuestAssertions.h"
#include "QuestScenarios.h"
#include "QuestTest.h"

#include "../../lib/mapObjects/CGCreature.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/mapObjects/MiscObjects.h"
#include "../../lib/mapObjects/army/CStackBasicDescriptor.h"

// Phase 2.1 — loader → CQuest/Limiter structural tests. One per scenario in
// QuestScenarios that fits SOD format. HOTA-format scenarios (hero class,
// reach date, repeatable) and the cross-format placeholder kill-quest tests
// will land alongside their respective Phase 4 follow-ups.
//
// Each test:
//   1. Builds the scenario inline via QuestScenarios::xxx().
//   2. Loads it through QuestTest::startWithMap (TinyH3M -> CMapLoaderH3M).
//   3. Locates the produced CGSeerHut / CGQuestGuard by position.
//   4. Asserts the produced CQuest::mission matches the expected Limiter
//      via EXPECT_QUEST_MISSION (per-field diff messages on mismatch).

using namespace QuestScenarios;

class QuestLoaderTest : public QuestTest {};

namespace
{
// Test-side mirror of the IDs hard-coded inside QuestScenarios.cpp. Kept in
// sync manually — if either side changes the matching loader test will fail
// immediately rather than passing silently against the wrong identifier.
constexpr int kHeroChristian      = 6;
constexpr int kHeroTyris          = 7;
constexpr int kCreatureGriffin    = 4;
constexpr int kArtifactSash       = 68;
constexpr int kArtifactHelm       = 130;

template<class T>
const T * expectAt(const QuestTest & f, const int3 & pos)
{
	auto * obj = const_cast<QuestTest &>(f).findObjectAt(pos);
	EXPECT_NE(obj, nullptr) << "no object at " << pos.toString();
	const auto * casted = dynamic_cast<const T *>(obj);
	EXPECT_NE(casted, nullptr) << "object at " << pos.toString() << " has unexpected dynamic type";
	return casted;
}
} // namespace

TEST_F(QuestLoaderTest, QuestSeerArtifact_loadsArtifactLimiter)
{
	auto s = seerArtifact();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind = EQuestMission::ARTIFACT;
	expected.limiter.artifacts.push_back(ArtifactID(kArtifactSash));
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestSeerArtifactAssembled_loadsArtifactLimiter)
{
	auto s = seerArtifactAssembled();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind = EQuestMission::ARTIFACT;
	expected.limiter.artifacts.push_back(ArtifactID(kArtifactHelm));
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestSeerArmy_loadsCreatureLimiter)
{
	auto s = seerArmy();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind = EQuestMission::ARMY;
	expected.limiter.creatures.emplace_back(CreatureID(kCreatureGriffin), 5);
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestSeerResources_loadsResourceLimiter)
{
	auto s = seerResources();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind = EQuestMission::RESOURCES;
	expected.limiter.resources[GameResID::WOOD] = 5;
	expected.limiter.resources[GameResID::GOLD] = 5000;
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestSeerHero_loadsHeroLimiter)
{
	auto s = seerHero();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind = EQuestMission::HERO;
	expected.limiter.heroes.push_back(HeroTypeID(kHeroTyris));
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestSeerPlayer_loadsPlayerLimiter)
{
	auto s = seerPlayer();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind = EQuestMission::PLAYER;
	expected.limiter.players.push_back(PlayerColor(1));
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestSeerLevel_loadsLevelLimiter)
{
	auto s = seerLevel();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * easy = expectAt<CGSeerHut>(*this, s.questPos);
	const auto * hard = expectAt<CGSeerHut>(*this, s.questPos2);

	ExpectedMission expectedEasy;
	expectedEasy.kind             = EQuestMission::LEVEL;
	expectedEasy.limiter.heroLevel = 3;
	EXPECT_QUEST_MISSION(easy->getQuest(), expectedEasy);

	ExpectedMission expectedHard;
	expectedHard.kind             = EQuestMission::LEVEL;
	expectedHard.limiter.heroLevel = 10;
	EXPECT_QUEST_MISSION(hard->getQuest(), expectedHard);
}

TEST_F(QuestLoaderTest, QuestSeerPrimarySkill_loadsPrimaryLimiter)
{
	auto s = seerPrimarySkill();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * easy = expectAt<CGSeerHut>(*this, s.questPos);
	const auto * hard = expectAt<CGSeerHut>(*this, s.questPos2);

	ExpectedMission expectedEasy;
	expectedEasy.kind            = EQuestMission::PRIMARY_SKILL;
	expectedEasy.limiter.primary = {3, 0, 0, 0};
	EXPECT_QUEST_MISSION(easy->getQuest(), expectedEasy);

	ExpectedMission expectedHard;
	expectedHard.kind            = EQuestMission::PRIMARY_SKILL;
	expectedHard.limiter.primary = {10, 0, 0, 0};
	EXPECT_QUEST_MISSION(hard->getQuest(), expectedHard);
}

TEST_F(QuestLoaderTest, QuestSeerKillCreature_loadsKillTarget)
{
	// Kill-creature missions store the wire identifier in killTarget, resolved
	// to an ObjectInstanceID by afterRead. EXPECT_QUEST_MISSION's KILL_CREATURE
	// branch checks the slot is populated; this test additionally verifies the
	// resolved target points at the actual monster placed by the scenario.
	auto s = seerKillCreature();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer    = expectAt<CGSeerHut>     (*this, s.questPos);
	const auto * monster = expectAt<CGCreature>    (*this, s.secondHeroPos);
	ASSERT_NE(seer,    nullptr);
	ASSERT_NE(monster, nullptr);

	ExpectedMission expected;
	expected.kind = EQuestMission::KILL_CREATURE;
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
	EXPECT_EQ(seer->getQuest().killTarget, monster->id);
}

TEST_F(QuestLoaderTest, QuestSeerKillHero_loadsKillTarget)
{
	auto s = seerKillHero();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer   = expectAt<CGSeerHut>     (*this, s.questPos);
	const auto * target = expectAt<CGHeroInstance>(*this, s.secondHeroPos);
	ASSERT_NE(seer,   nullptr);
	ASSERT_NE(target, nullptr);

	ExpectedMission expected;
	expected.kind = EQuestMission::KILL_HERO;
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
	EXPECT_EQ(seer->getQuest().killTarget, target->id);
}

TEST_F(QuestLoaderTest, QuestSeerTimeout_loadsLastDay)
{
	auto s = seerTimeout();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectAt<CGSeerHut>(*this, s.questPos);

	// Timeout scenario uses a trivial 1-wood limiter — the loader-side check
	// is the lastDay slot, which Phase 2.2 will exercise in the runtime
	// expiry test.
	ExpectedMission expected;
	expected.kind                                = EQuestMission::RESOURCES;
	expected.limiter.resources[GameResID::WOOD]  = 1;
	expected.lastDay                             = 7;
	EXPECT_QUEST_MISSION(seer->getQuest(), expected);
}

TEST_F(QuestLoaderTest, QuestGuard_loadsLimiterAndRemoveObject)
{
	// scenarioQuestGuard() places a quest guard demanding 1000 wood. The
	// loader builds a CGQuestGuard (distinct dynamic type from CGSeerHut)
	// with a ResourceLimiter — the "and removeObject" half of the name is the
	// runtime acceptance flow, validated in Phase 2.3.
	auto s = questGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * guard = expectAt<CGQuestGuard>(*this, s.questPos);

	ExpectedMission expected;
	expected.kind                                = EQuestMission::RESOURCES;
	expected.limiter.resources[GameResID::WOOD]  = 1000;
	EXPECT_QUEST_MISSION(guard->getQuest(), expected);
}
