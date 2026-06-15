/*
 * CQuestScenariosSmokeTest.cpp, part of VCMI engine
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

// Phase 1 smoke harness: every scenario factory is exercised end-to-end via
// startWithMap so a malformed builder (e.g. wire layout drifted from the
// loader) trips immediately rather than waiting for the Phase 2 test that
// actually asserts on its quest state. The assertion bar is intentionally
// shallow — "loaded a non-null CMap, found the expected object at the
// expected position". Deep behaviour goes in CQuest*Test.cpp files later.

using namespace QuestScenarios;

class QuestScenariosSmokeTest : public QuestTest
{
protected:
	template<class T>
	const T * expectObjectAt(const int3 & pos)
	{
		auto * obj = findObjectAt(pos);
		EXPECT_NE(obj, nullptr) << "no object at " << pos.toString();
		const auto * casted = dynamic_cast<const T *>(obj);
		EXPECT_NE(casted, nullptr) << "object at " << pos.toString() << " has unexpected dynamic type";
		return casted;
	}
};

TEST_F(QuestScenariosSmokeTest, SeerArtifact)
{
	auto s = seerArtifact();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGSeerHut>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, SeerArtifactAssembled)
{
	auto s = seerArtifactAssembled();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGSeerHut>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, SeerArmy)
{
	auto s = seerArmy();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * hero = expectObjectAt<CGHeroInstance>(s.heroPos);
	ASSERT_NE(hero, nullptr);
	EXPECT_TRUE(hero->hasStackAtSlot(SlotID(0)));  // garrison present
	expectObjectAt<CGSeerHut>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, SeerResources)
{
	auto s = seerResources();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGSeerHut>(s.questPos);
	// grantResources actually mutates PlayerState — proves the helper compiles.
	grantResources(PlayerColor(0), GameResID(GameResID::GOLD), 7000);
}

TEST_F(QuestScenariosSmokeTest, SeerHero)
{
	auto s = seerHero();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGHeroInstance>(s.secondHeroPos);
	expectObjectAt<CGSeerHut>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, SeerPlayer)
{
	auto s = seerPlayer();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * red  = expectObjectAt<CGHeroInstance>(s.heroPos);
	const auto * blue = expectObjectAt<CGHeroInstance>(s.secondHeroPos);
	ASSERT_NE(red,  nullptr);
	ASSERT_NE(blue, nullptr);
	EXPECT_EQ(red->getOwner(),  PlayerColor(0));
	EXPECT_EQ(blue->getOwner(), PlayerColor(1));
	expectObjectAt<CGSeerHut>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, SeerLevel)
{
	auto s = seerLevel();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * hero = expectObjectAt<CGHeroInstance>(s.heroPos);
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->exp, 10000);
	expectObjectAt<CGSeerHut>(s.questPos);
	expectObjectAt<CGSeerHut>(s.questPos2);
}

TEST_F(QuestScenariosSmokeTest, SeerPrimarySkill)
{
	auto s = seerPrimarySkill();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGSeerHut>(s.questPos);
	expectObjectAt<CGSeerHut>(s.questPos2);
}

TEST_F(QuestScenariosSmokeTest, SeerKillCreature)
{
	auto s = seerKillCreature();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGCreature>(s.secondHeroPos);
	const auto * seer = expectObjectAt<CGSeerHut>(s.questPos);
	ASSERT_NE(seer, nullptr);
	// Kill-target was resolved from a non-zero wire identifier during afterRead.
	EXPECT_TRUE(seer->getQuest().killTarget.hasValue());
}

TEST_F(QuestScenariosSmokeTest, SeerKillHero)
{
	auto s = seerKillHero();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGHeroInstance>(s.secondHeroPos);
	const auto * seer = expectObjectAt<CGSeerHut>(s.questPos);
	ASSERT_NE(seer, nullptr);
	EXPECT_TRUE(seer->getQuest().killTarget.hasValue());
}

TEST_F(QuestScenariosSmokeTest, SeerTimeout)
{
	auto s = seerTimeout();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * seer = expectObjectAt<CGSeerHut>(s.questPos);
	ASSERT_NE(seer, nullptr);
	EXPECT_EQ(seer->getQuest().lastDay, 7);
}

TEST_F(QuestScenariosSmokeTest, SeerEmptyArmyToggle)
{
	auto s = seerEmptyArmyToggle();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	const auto * hero = expectObjectAt<CGHeroInstance>(s.heroPos);
	ASSERT_NE(hero, nullptr);
	EXPECT_TRUE(hero->hasStackAtSlot(SlotID(0)));
	expectObjectAt<CGSeerHut>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, QuestGuard)
{
	auto s = questGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGQuestGuard>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, QuestGuardBlockVisit)
{
	auto s = questGuardBlockVisit();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGQuestGuard>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, QuestKeymasterTent)
{
	auto s = questKeymasterTent();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGKeymasterTent>(s.questPos);
}

TEST_F(QuestScenariosSmokeTest, QuestBorderGuard)
{
	auto s = questBorderGuard();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGKeymasterTent>(s.questPos);
	expectObjectAt<CGBorderGuard>(s.questPos2);
}

TEST_F(QuestScenariosSmokeTest, QuestBorderGate)
{
	auto s = questBorderGate();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGKeymasterTent>(s.questPos);
	// CGBorderGate is the gate flavour — distinct class from CGBorderGuard.
	auto * gate = findObjectAt(s.questPos2);
	ASSERT_NE(gate, nullptr);
	EXPECT_NE(dynamic_cast<CGBorderGate *>(gate), nullptr);
}

TEST_F(QuestScenariosSmokeTest, DisabledQuestBorderMultiSibling)
{
	auto s = disabledQuestBorderMultiSibling();
	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(s.builder)));
	expectObjectAt<CGHeroInstance>(s.heroPos);
	expectObjectAt<CGKeymasterTent>(s.questPos);
	expectObjectAt<CGBorderGuard>(s.questPos2);
	expectObjectAt<CGBorderGuard>(s.questPos3);
}
