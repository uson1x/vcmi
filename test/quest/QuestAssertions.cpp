/*
 * QuestAssertions.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "QuestAssertions.h"

#include "../../lib/mapObjects/army/CStackBasicDescriptor.h"

namespace quest_test
{

namespace
{

testing::AssertionResult artifactsMatch(const Rewardable::Limiter & a, const Rewardable::Limiter & b)
{
	if(a.artifacts == b.artifacts)
		return testing::AssertionSuccess();

	auto fmt = [](const std::vector<ArtifactID> & v)
	{
		std::ostringstream out;
		out << "[";
		for(size_t i = 0; i < v.size(); ++i)
			out << (i ? ", " : "") << v[i].getNum();
		out << "]";
		return out.str();
	};
	return testing::AssertionFailure()
		<< "  expected artifacts " << fmt(a.artifacts)
		<< "\n  actual artifacts   " << fmt(b.artifacts);
}

testing::AssertionResult creaturesMatch(const Rewardable::Limiter & a, const Rewardable::Limiter & b)
{
	if(a.creatures.size() != b.creatures.size())
		return testing::AssertionFailure()
			<< "  expected " << a.creatures.size() << " creature stacks, actual " << b.creatures.size();

	for(size_t i = 0; i < a.creatures.size(); ++i)
	{
		if(a.creatures[i].getId() != b.creatures[i].getId() || a.creatures[i].getCount() != b.creatures[i].getCount())
		{
			return testing::AssertionFailure()
				<< "  stack[" << i << "] expected (id=" << a.creatures[i].getId().getNum() << ", count=" << a.creatures[i].getCount()
				<< "), actual (id=" << b.creatures[i].getId().getNum() << ", count=" << b.creatures[i].getCount() << ")";
		}
	}
	return testing::AssertionSuccess();
}

testing::AssertionResult resourcesMatch(const Rewardable::Limiter & a, const Rewardable::Limiter & b)
{
	if(a.resources == b.resources)
		return testing::AssertionSuccess();

	std::ostringstream out;
	out << "\n  expected resources [";
	for(int i = 0; i < GameResID::COUNT; ++i)
		out << (i ? "," : "") << a.resources[GameResID(i)];
	out << "]\n  actual resources   [";
	for(int i = 0; i < GameResID::COUNT; ++i)
		out << (i ? "," : "") << b.resources[GameResID(i)];
	out << "]";
	return testing::AssertionFailure() << out.str();
}

testing::AssertionResult primaryMatch(const Rewardable::Limiter & a, const Rewardable::Limiter & b)
{
	if(a.primary == b.primary)
		return testing::AssertionSuccess();
	return testing::AssertionFailure() << "  primary skill vector differs (expected size=" << a.primary.size() << " vs actual size=" << b.primary.size() << ")";
}

testing::AssertionResult heroesMatch(const Rewardable::Limiter & a, const Rewardable::Limiter & b)
{
	if(a.heroes == b.heroes)
		return testing::AssertionSuccess();
	return testing::AssertionFailure() << "  hero list differs (expected " << a.heroes.size() << " heroes, actual " << b.heroes.size() << ")";
}

testing::AssertionResult playersMatch(const Rewardable::Limiter & a, const Rewardable::Limiter & b)
{
	if(a.players == b.players)
		return testing::AssertionSuccess();
	return testing::AssertionFailure() << "  player list differs (expected " << a.players.size() << " players, actual " << b.players.size() << ")";
}

} // namespace

void expectQuestMission(const CQuest & actual, const ExpectedMission & expected,
                        const char * file, int line)
{
	// Source-pin every failure to the call site so tests can be navigated
	// from the IDE / test runner output.
	::testing::ScopedTrace trace(file, line, "EXPECT_QUEST_MISSION");

	if(actual.lastDay != expected.lastDay)
		ADD_FAILURE() << "lastDay: expected " << expected.lastDay << ", actual " << actual.lastDay;

	// Limiter comparison is gated per-field on what the mission kind populates.
	// Rationale: a plain `Limiter == Limiter` would also work (default-constructed
	// Limiters agree on every field, and the SOD loader only writes the
	// mission-specific one), but it produces uninformative "Limiter mismatch"
	// failures with no hint about which field disagreed. Per-field comparison
	// produces a readable diff. Trade-off: if a future regression makes the
	// loader populate a field outside the mission-specific one, this helper
	// will not flag it.
	switch(expected.kind)
	{
		case EQuestMission::ARTIFACT:
			EXPECT_TRUE(artifactsMatch(expected.limiter, actual.mission));
			break;
		case EQuestMission::ARMY:
			EXPECT_TRUE(creaturesMatch(expected.limiter, actual.mission));
			break;
		case EQuestMission::RESOURCES:
			EXPECT_TRUE(resourcesMatch(expected.limiter, actual.mission));
			break;
		case EQuestMission::PRIMARY_SKILL:
			EXPECT_TRUE(primaryMatch(expected.limiter, actual.mission));
			break;
		case EQuestMission::LEVEL:
			EXPECT_EQ(expected.limiter.heroLevel, actual.mission.heroLevel);
			break;
		case EQuestMission::HERO:
			EXPECT_TRUE(heroesMatch(expected.limiter, actual.mission));
			break;
		case EQuestMission::PLAYER:
			EXPECT_TRUE(playersMatch(expected.limiter, actual.mission));
			break;
		case EQuestMission::KILL_CREATURE:
		case EQuestMission::KILL_HERO:
			// killTarget is resolved from a wire identifier in afterRead, so
			// the test only asserts the slot got populated (the specific id
			// depends on object load order and is not stable across edits).
			EXPECT_TRUE(actual.killTarget.hasValue())
				<< "kill-quest target was not resolved to an ObjectInstanceID";
			break;
		case EQuestMission::NONE:
			// Nothing to compare beyond lastDay.
			break;
		default:
			ADD_FAILURE() << "expectQuestMission: mission kind "
				<< static_cast<int>(expected.kind) << " not handled";
			break;
	}

	if(!expected.firstVisitText.empty())
		EXPECT_EQ(expected.firstVisitText, actual.firstVisitText.toString());
	if(!expected.nextVisitText.empty())
		EXPECT_EQ(expected.nextVisitText, actual.nextVisitText.toString());
	if(!expected.completedText.empty())
		EXPECT_EQ(expected.completedText, actual.completedText.toString());
}

} // namespace quest_test
