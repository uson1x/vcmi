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
#include "QuestTest.h"

#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CQuest.h"

using TinyH3M::TinyH3MBuilder;

// Phase 0 smoke test: prove that the infrastructure stack (TinyH3MBuilder ->
// MapServiceTinyH3M -> QuestTest -> EXPECT_QUEST_MISSION) is wired correctly.
// Per-mission loader coverage is the job of Phase 2.1.
class QuestLoaderSmokeTest : public QuestTest {};

TEST_F(QuestLoaderSmokeTest, ResourcesMissionLimiterRoundTrips)
{
	TinyH3MBuilder scenario(EMapFormat::SOD);
	scenario
		.size(36, /*twoLevel*/ false)
		.name("ResourcesSmoke")
		.playerActive(PlayerColor(0))
		.hero({5, 5, 0}, HeroTypeID(0), PlayerColor(0))
		.questGuard({10, 10, 0}, TinyH3MBuilder::missionResources({0, 0, 0, 0, 0, 0, 1000})
			.withLastDay(7));

	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(scenario)));

	const auto * guard = findFirst<CGQuestGuard>();
	ASSERT_NE(guard, nullptr);

	ExpectedMission expected;
	expected.kind                                = EQuestMission::RESOURCES;
	expected.limiter.resources[GameResID::GOLD]  = 1000;
	expected.lastDay                             = 7;

	EXPECT_QUEST_MISSION(guard->getQuest(), expected);
}

TEST_F(QuestLoaderSmokeTest, ArtifactsMissionLimiterRoundTrips)
{
	TinyH3MBuilder scenario(EMapFormat::SOD);
	scenario
		.size(36, /*twoLevel*/ false)
		.name("ArtifactsSmoke")
		.playerActive(PlayerColor(0))
		.hero({5, 5, 0}, HeroTypeID(0), PlayerColor(0))
		.questGuard({10, 10, 0}, TinyH3MBuilder::missionArtifacts({ArtifactID(7)}));

	ASSERT_NO_FATAL_FAILURE(startWithMap(std::move(scenario)));

	const auto * guard = findFirst<CGQuestGuard>();
	ASSERT_NE(guard, nullptr);

	ExpectedMission expected;
	expected.kind = EQuestMission::ARTIFACT;
	expected.limiter.artifacts.push_back(ArtifactID(7));

	EXPECT_QUEST_MISSION(guard->getQuest(), expected);
}
