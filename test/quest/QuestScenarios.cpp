/*
 * QuestScenarios.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "QuestScenarios.h"

namespace QuestScenarios
{

using TinyH3M::TinyH3MBuilder;

namespace
{

// Stable map size + a fresh, named SOD builder with red active. Every scenario
// starts from this skeleton; per-scenario differences live below it.
TinyH3MBuilder fresh(const std::string & name)
{
	TinyH3MBuilder b(EMapFormat::SOD);
	b.size(36, /*twoLevel*/ false)
	 .name(name)
	 .playerActive(PlayerColor(0));
	return b;
}

// Common identifiers used across scenarios. Deliberately avoid id 0/1 for
// hero/creature: zero-initialised state has historically been misread as
// "Orrin" or "pikeman", masking field-initialisation bugs. Using ids that no
// default-constructed value would land on makes those failures visible.
constexpr int kHeroChristian   = 6;   // knight class, like Orrin but non-zero
constexpr int kHeroTyris       = 7;   // second knight for two-hero scenarios
constexpr int kCreatureGriffin     = 4;
constexpr int kCreatureRoyalGriffin = 5;
constexpr int kCreatureImp     = 42;

constexpr int kArtifactCentaurAxe   = 7;   // generic small artifact for misc tests
constexpr int kArtifactSash         = 68;  // ambassadorsSash
constexpr int kArtifactAngelicAlly  = 129; // angelicAlliance (combo) — sword slot
constexpr int kArtifactHelm         = 36;  // helmOfHeavenlyEnlightenment (component of alliance)

} // namespace

Scenario seerArtifact()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("SeerArtifact");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
			.heroBackpack({ArtifactID(kArtifactSash)})
		.seerHut(s.questPos,
			TinyH3MBuilder::missionArtifacts({ArtifactID(kArtifactSash)}),
			TinyH3MBuilder::rewardExperience(1000));
	return s;
}

Scenario seerArtifactAssembled()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("SeerArtifactAssembled");
	// Angelic Alliance lives in the backpack rather than its natural RIGHT_HAND
	// equipped slot. Rewardable::Limiter::heroAllowed only walks combined-
	// artifact parts when the assembly is in the backpack; if the assembly is
	// worn, the limiter sees only the assembly's own type id and refuses the
	// "bring me the Helm component" mission. Putting it in backpack still
	// exercises completeQuest's disassemble + take path, which is the point of
	// this scenario.
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
			.heroBackpack({ArtifactID(kArtifactAngelicAlly)})
		.seerHut(s.questPos,
			TinyH3MBuilder::missionArtifacts({ArtifactID(kArtifactHelm)}),
			TinyH3MBuilder::rewardExperience(1000));
	return s;
}

Scenario seerArmy()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("SeerArmy");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
			.heroGarrison({{CreatureID(kCreatureGriffin),      10},
			               {CreatureID(kCreatureRoyalGriffin),  5}})
		.seerHut(s.questPos,
			TinyH3MBuilder::missionArmy({{CreatureID(kCreatureGriffin), 5}}),
			TinyH3MBuilder::rewardExperience(500));
	return s;
}

Scenario seerResources()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("SeerResources");
	// Bring 5000 gold + 5 wood. Player starting resources are zero — the test
	// is expected to grantResources(RED, GOLD, 7000) + (WOOD, 10) post-load.
	std::array<uint32_t, 7> req{};
	req[GameResID::WOOD] = 5;
	req[GameResID::GOLD] = 5000;
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.seerHut(s.questPos,
			TinyH3MBuilder::missionResources(req),
			TinyH3MBuilder::rewardResource(GameResID(GameResID::MERCURY), 5));
	return s;
}

Scenario seerHero()
{
	Scenario s;
	s.heroPos       = {5, 5, 0};
	s.secondHeroPos = {6, 5, 0};
	s.questPos      = {10, 10, 0};
	s.builder       = fresh("SeerHero");
	s.builder
		.playerActive(PlayerColor(1))  // a second hero slot, owned by blue here
		.hero(s.heroPos,       HeroTypeID(kHeroChristian),     PlayerColor(0))
		.hero(s.secondHeroPos, HeroTypeID(kHeroTyris), PlayerColor(0))
		.seerHut(s.questPos,
			TinyH3MBuilder::missionHero(HeroTypeID(kHeroTyris)),
			TinyH3MBuilder::rewardExperience(500));
	return s;
}

Scenario seerPlayer()
{
	Scenario s;
	s.heroPos       = {5, 5, 0};
	s.secondHeroPos = {20, 20, 0};
	s.questPos      = {10, 10, 0};
	s.builder       = fresh("SeerPlayer");
	s.builder
		.playerActive(PlayerColor(1))
		.hero(s.heroPos,       HeroTypeID(kHeroChristian),     PlayerColor(0))
		.hero(s.secondHeroPos, HeroTypeID(kHeroTyris), PlayerColor(1))
		.seerHut(s.questPos,
			TinyH3MBuilder::missionPlayer(PlayerColor(1)),
			TinyH3MBuilder::rewardExperience(500));
	return s;
}

Scenario seerLevel()
{
	Scenario s;
	s.heroPos   = {5, 5, 0};
	s.questPos  = {10, 10, 0};   // easy seer (level >= 3)
	s.questPos2 = {15, 15, 0};   // hard seer (level >= 10)
	s.builder   = fresh("SeerLevel");
	// 10000 XP puts most heroes around level 5 in vanilla H3 progression.
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
			.heroExperience(10000)
		.seerHut(s.questPos,  TinyH3MBuilder::missionLevel(3),  TinyH3MBuilder::rewardExperience(100))
		.seerHut(s.questPos2, TinyH3MBuilder::missionLevel(10), TinyH3MBuilder::rewardExperience(100));
	return s;
}

Scenario seerPrimarySkill()
{
	Scenario s;
	s.heroPos   = {5, 5, 0};
	s.questPos  = {10, 10, 0};   // easy seer (attack >= 3)
	s.questPos2 = {15, 15, 0};   // hard seer (attack >= 10)
	s.builder   = fresh("SeerPrimarySkill");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
			.heroPrimary(/*attack*/ 5, /*defense*/ 0, /*sp*/ 0, /*knowledge*/ 0)
		.seerHut(s.questPos,
			TinyH3MBuilder::missionPrimarySkills(3, 0, 0, 0),
			TinyH3MBuilder::rewardExperience(100))
		.seerHut(s.questPos2,
			TinyH3MBuilder::missionPrimarySkills(10, 0, 0, 0),
			TinyH3MBuilder::rewardExperience(100));
	return s;
}

Scenario seerKillCreature()
{
	Scenario s;
	s.heroPos        = {5, 5, 0};
	s.secondHeroPos  = {12, 12, 0}; // monster position is reused as "secondary" for findObjectAt convenience
	s.questPos       = {10, 10, 0};
	s.builder        = fresh("SeerKillCreature");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.monster(s.secondHeroPos, CreatureID(kCreatureImp), /*count*/ 1);
	s.target = s.builder.lastHandle();
	s.builder
		.seerHut(s.questPos,
			TinyH3MBuilder::missionKillCreature(s.target),
			TinyH3MBuilder::rewardExperience(500));
	return s;
}

Scenario seerKillHero()
{
	Scenario s;
	s.heroPos       = {5, 5, 0};
	s.secondHeroPos = {20, 20, 0};
	s.questPos      = {10, 10, 0};
	s.builder       = fresh("SeerKillHero");
	s.builder
		.playerActive(PlayerColor(1))
		.hero(s.heroPos,       HeroTypeID(kHeroChristian),     PlayerColor(0))
		.hero(s.secondHeroPos, HeroTypeID(kHeroTyris), PlayerColor(1));
	s.target = s.builder.lastHandle();
	s.builder
		.seerHut(s.questPos,
			TinyH3MBuilder::missionKillHero(s.target),
			TinyH3MBuilder::rewardExperience(500));
	return s;
}

Scenario seerTimeout()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("SeerTimeout");
	// Trivial limiter (1 wood) so the test only exercises the lastDay
	// expiry path, not the limiter-evaluation path.
	std::array<uint32_t, 7> req{};
	req[GameResID::WOOD] = 1;
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.seerHut(s.questPos,
			TinyH3MBuilder::missionResources(req).withLastDay(7),
			TinyH3MBuilder::rewardExperience(100));
	return s;
}

Scenario seerEmptyArmyToggle()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("SeerEmptyArmyToggle");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
			.heroGarrison({{CreatureID(kCreatureGriffin), 1}})
		.seerHut(s.questPos,
			TinyH3MBuilder::missionArmy({{CreatureID(kCreatureGriffin), 1}}),
			TinyH3MBuilder::rewardNothing());
	return s;
}

Scenario questGuard()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("QuestGuard");
	std::array<uint32_t, 7> req{};
	req[GameResID::WOOD] = 1000;
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.questGuard(s.questPos, TinyH3MBuilder::missionResources(req));
	return s;
}

Scenario questGuardBlockVisit()
{
	Scenario s;
	s.heroPos  = {9, 10, 0};      // immediately west of the quest-guard tile
	s.questPos = {10, 10, 0};
	s.builder  = fresh("QuestGuardBlockVisit");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.questGuard(s.questPos);   // NONE mission — the test exercises blockVisit alone
	return s;
}

Scenario questKeymasterTent()
{
	Scenario s;
	s.heroPos  = {5, 5, 0};
	s.questPos = {10, 10, 0};
	s.builder  = fresh("QuestKeymasterTent");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.keymaster(s.questPos, /*color*/ 0);
	return s;
}

Scenario questBorderGuard()
{
	Scenario s;
	s.heroPos   = {5, 5, 0};
	s.questPos  = {10, 10, 0};      // keymaster
	s.questPos2 = {15, 15, 0};      // border guard, same colour
	s.builder   = fresh("QuestBorderGuard");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.keymaster  (s.questPos,  /*color*/ 0)
		.borderGuard(s.questPos2, /*color*/ 0);
	return s;
}

Scenario questBorderGate()
{
	Scenario s;
	s.heroPos   = {5, 5, 0};
	s.questPos  = {10, 10, 0};
	s.questPos2 = {15, 15, 0};
	s.builder   = fresh("QuestBorderGate");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.keymaster (s.questPos,  /*color*/ 0)
		.borderGate(s.questPos2, /*color*/ 0);
	return s;
}

Scenario disabledQuestBorderMultiSibling()
{
	Scenario s;
	s.heroPos   = {5, 5, 0};
	s.questPos  = {10, 10, 0};      // keymaster
	s.questPos2 = {15, 15, 0};      // first border guard
	s.questPos3 = {20, 20, 0};      // second border guard, same colour
	s.builder   = fresh("DisabledQuestBorderMultiSibling");
	s.builder
		.hero(s.heroPos, HeroTypeID(kHeroChristian), PlayerColor(0))
		.keymaster  (s.questPos,  /*color*/ 0)
		.borderGuard(s.questPos2, /*color*/ 0)
		.borderGuard(s.questPos3, /*color*/ 0);
	return s;
}

} // namespace QuestScenarios
