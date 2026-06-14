/*
 * TinyH3MBuilderTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "../mock/TinyH3MBuilder.h"

#include "../../lib/callback/EditorCallback.h"
#include "../../lib/entities/artifact/CArtifactInstance.h"
#include "../../lib/filesystem/CMemoryBuffer.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/mapObjects/MiscObjects.h"
#include "../../lib/mapping/CMap.h"
#include "../../lib/mapping/CMapHeader.h"
#include "../../lib/mapping/MapFormat.h"
#include "../../lib/mapping/MapFormatH3M.h"
#include "../../lib/rewardable/Configuration.h"
#include "../../lib/rewardable/Limiter.h"
#include "../../lib/rewardable/Reward.h"

namespace
{
auto loadHeader(std::vector<uint8_t> bytes)
{
	CMemoryBuffer buf;
	buf.write(bytes.data(), static_cast<si64>(bytes.size()));
	buf.seek(0);

	// modName "core" so readLocalizedString -> mapRegisterLocalizedString can resolve mod language
	CMapLoaderH3M loader("TinyH3MBuilderTest", "core", "ASCII", &buf);
	return loader.loadMapHeader();
}

// Returned aggregate keeps the backing buffer and EditorCallback alive for the lifetime of the CMap.
struct LoadedMap
{
	std::unique_ptr<CMap>           map;
	std::unique_ptr<CMemoryBuffer>  stream;
	std::unique_ptr<EditorCallback> cb;
};

LoadedMap loadMap(std::vector<uint8_t> bytes)
{
	LoadedMap r;
	r.stream = std::make_unique<CMemoryBuffer>();
	r.stream->write(bytes.data(), static_cast<si64>(bytes.size()));
	r.stream->seek(0);
	// Same callback the mapeditor uses — provides the non-null IGameInfoCallback that
	// per-type object handlers (Keymaster / Border Guard / ...) assert on in their factory.
	r.cb = std::make_unique<EditorCallback>(nullptr);

	CMapLoaderH3M loader("TinyH3MBuilderTest", "core", "ASCII", r.stream.get());
	r.map = loader.loadMap(r.cb.get());
	r.cb->setMap(r.map.get());
	return r;
}
}

TEST(TinyH3MBuilderTest, EmptyROEHeaderRoundTrips)
{
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::ROE)
		.size(36, /*twoLevel*/ false)
		.name("EmptyROE")
		.description("Empty ROE map")
		.difficulty(EMapDifficulty::NORMAL)
		.buildAndDump("EmptyROEHeaderRoundTrips");

	auto header = loadHeader(std::move(bytes));
	ASSERT_NE(header, nullptr);

	EXPECT_EQ(header->version, EMapFormat::ROE);
	EXPECT_EQ(header->width, 36);
	EXPECT_EQ(header->height, 36);
	EXPECT_EQ(header->mapLayers.size(), 1u);
	EXPECT_EQ(header->difficulty, EMapDifficulty::NORMAL);
	EXPECT_EQ(header->players.size(), static_cast<size_t>(PlayerColor::PLAYER_LIMIT_I));
	for(const auto & p : header->players)
	{
		EXPECT_FALSE(p.canHumanPlay);
		EXPECT_FALSE(p.canComputerPlay);
	}
}

TEST(TinyH3MBuilderTest, TwoLevelMapHasTwoLayers)
{
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::ROE)
		.size(36, /*twoLevel*/ true)
		.name("TwoLevelROE")
		.buildAndDump("TwoLevelMapHasTwoLayers");

	auto header = loadHeader(std::move(bytes));
	ASSERT_NE(header, nullptr);
	EXPECT_EQ(header->mapLayers.size(), 2u);
}

TEST(TinyH3MBuilderTest, EmptyROEFullLoad)
{
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::ROE)
		.size(36, /*twoLevel*/ false)
		.name("EmptyROEFull")
		.difficulty(EMapDifficulty::NORMAL)
		.buildAndDump("EmptyROEFullLoad");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);
	EXPECT_EQ(loaded.map->width, 36);
	EXPECT_EQ(loaded.map->height, 36);
	EXPECT_EQ(loaded.map->levels(), 1);
	EXPECT_TRUE(loaded.map->getHeroesOnMap().empty());
}

TEST(TinyH3MBuilderTest, EmptySODHeaderRoundTrips)
{
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("EmptySOD")
		.description("Empty SOD map")
		.difficulty(EMapDifficulty::NORMAL)
		.buildAndDump("EmptySODHeaderRoundTrips");

	auto header = loadHeader(std::move(bytes));
	ASSERT_NE(header, nullptr);
	EXPECT_EQ(header->version, EMapFormat::SOD);
	EXPECT_EQ(header->width, 36);
	EXPECT_EQ(header->height, 36);
	EXPECT_EQ(header->mapLayers.size(), 1u);
	EXPECT_EQ(header->difficulty, EMapDifficulty::NORMAL);
	for(const auto & p : header->players)
	{
		EXPECT_FALSE(p.canHumanPlay);
		EXPECT_FALSE(p.canComputerPlay);
	}
}

TEST(TinyH3MBuilderTest, EmptySODFullLoad)
{
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("EmptySODFull")
		.difficulty(EMapDifficulty::NORMAL)
		.buildAndDump("EmptySODFullLoad");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);
	EXPECT_EQ(loaded.map->width, 36);
	EXPECT_EQ(loaded.map->height, 36);
	EXPECT_EQ(loaded.map->levels(), 1);
	EXPECT_TRUE(loaded.map->getHeroesOnMap().empty());
}

TEST(TinyH3MBuilderTest, AllSupportedObjectsLoad)
{
	// One instance of every object kind the builder currently emits. Acceptance
	// bar is "CMapLoaderH3M::loadMap returns a non-null CMap" — per-type body
	// assertions live in dedicated tests.
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("AllObjects")
		.playerActive(PlayerColor(0))
		.randomTown ({ 5,  5, 0}, PlayerColor(0))
		.monster    ({10, 10, 0}, CreatureID(27), /*count*/ 7) // Gold Dragon
		.resource   ({11, 10, 0}, GameResID(GameResID::GOLD), /*amount*/ 1000)
		.artifact   ({12, 10, 0}, ArtifactID(7))               // Centaur Axe — first artifact with a map sprite
		.keymaster  ({15, 15, 0}, /*color*/ 0)
		.borderGuard({16, 15, 0}, /*color*/ 0)
		.borderGate ({17, 15, 0}, /*color*/ 0)
		.questGuard ({20, 20, 0})
		.seerHut    ({22, 22, 0})
		.buildAndDump("AllSupportedObjectsLoad");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);
	EXPECT_EQ(loaded.map->width, 36);
	// 1 town + 1 monster + 1 resource + 1 artifact + 3 border + 1 quest guard + 1 seer = 9
	size_t live = 0;
	for(const auto & obj : loaded.map->objects)
		if(obj) ++live;
	EXPECT_EQ(live, 9u);
}

namespace
{
template<class T>
const T * findFirst(const CMap & map)
{
	for(const auto & obj : map.objects)
	{
		if(const auto * casted = dynamic_cast<const T *>(obj.get()))
			return casted;
	}
	return nullptr;
}

template<class T>
std::vector<const T *> findAll(const CMap & map)
{
	std::vector<const T *> out;
	for(const auto & obj : map.objects)
	{
		if(const auto * casted = dynamic_cast<const T *>(obj.get()))
			out.push_back(casted);
	}
	return out;
}
}

TEST(TinyH3MBuilderTest, HeroesPlacement)
{
	// Fixed hero (Orrin, type 0) owned by red, plus a random hero owned by blue.
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("HeroesPlacement")
		.playerActive(PlayerColor(0))
		.playerActive(PlayerColor(1))
		.hero      ({5, 5, 0}, HeroTypeID(0), PlayerColor(0))
		.randomHero({6, 6, 0}, PlayerColor(1))
		.buildAndDump("HeroesPlacement");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);

	const auto heroes = findAll<CGHeroInstance>(*loaded.map);
	ASSERT_EQ(heroes.size(), 2u);

	const CGHeroInstance * fixed = nullptr;
	const CGHeroInstance * random = nullptr;
	for(const auto * h : heroes)
	{
		if(h->getOwner() == PlayerColor(0))
			fixed = h;
		else if(h->getOwner() == PlayerColor(1))
			random = h;
	}

	ASSERT_NE(fixed, nullptr);
	ASSERT_NE(random, nullptr);
	EXPECT_EQ(fixed->getHeroTypeID(), HeroTypeID(0));
	EXPECT_EQ(fixed->anchorPos(), int3(5, 5, 0));
	EXPECT_EQ(random->anchorPos(), int3(6, 6, 0));
}

TEST(TinyH3MBuilderTest, SpellScrollLoads)
{
	// SpellID 15 = Magic Arrow (always available, no expansion required).
	const SpellID spell{15};

	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("ScrollMagicArrow")
		.scroll({10, 10, 0}, spell)
		.buildAndDump("SpellScrollLoads");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);

	const auto * scroll = findFirst<CGArtifact>(*loaded.map);
	ASSERT_NE(scroll, nullptr);
	EXPECT_EQ(scroll->anchorPos(), int3(10, 10, 0));

	const auto * inst = scroll->getArtifactInstance();
	ASSERT_NE(inst, nullptr);
	EXPECT_EQ(inst->getScrollSpellID(), spell);
}

TEST(TinyH3MBuilderTest, QuestGuardMissions)
{
	// Each quest-guard variant covered once. Asserts the loader populates the
	// matching field on CQuest::mission and clears lastDay (-1 = no timeout).
	using B = TinyH3M::TinyH3MBuilder;
	const ArtifactID centaurAxe{7};

	auto bytes = B(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("QuestMissions")
		.questGuard({ 5,  5, 0}, B::missionArtifacts({centaurAxe}))
		.questGuard({ 6,  5, 0}, B::missionArmy({{CreatureID(27), 5}}))   // 5 Gold Dragons
		.questGuard({ 7,  5, 0}, B::missionResources({0, 0, 0, 0, 0, 0, 1000})) // 1000 gold
		.questGuard({ 8,  5, 0}, B::missionPrimarySkills(3, 0, 0, 0))     // attack 3
		.questGuard({ 9,  5, 0}, B::missionLevel(5))
		.questGuard({10,  5, 0}, B::missionHero(HeroTypeID(0)))
		.questGuard({11,  5, 0}, B::missionPlayer(PlayerColor(2)))
		.buildAndDump("QuestGuardMissions");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);

	const auto guards = findAll<CGQuestGuard>(*loaded.map);
	ASSERT_EQ(guards.size(), 7u);

	std::map<int3, const CGQuestGuard *> byPos;
	for(const auto * g : guards)
		byPos[g->anchorPos()] = g;

	{
		const auto & q = byPos.at({5, 5, 0})->getQuest();
		EXPECT_EQ(q.lastDay, -1);
		ASSERT_EQ(q.mission.artifacts.size(), 1u);
		EXPECT_EQ(q.mission.artifacts[0], centaurAxe);
	}
	{
		const auto & q = byPos.at({6, 5, 0})->getQuest();
		ASSERT_EQ(q.mission.creatures.size(), 1u);
		EXPECT_EQ(q.mission.creatures[0].getId(), CreatureID(27));
		EXPECT_EQ(q.mission.creatures[0].getCount(), 5);
	}
	{
		const auto & q = byPos.at({7, 5, 0})->getQuest();
		EXPECT_EQ(q.mission.resources[GameResID::GOLD], 1000);
	}
	{
		const auto & q = byPos.at({8, 5, 0})->getQuest();
		ASSERT_EQ(q.mission.primary.size(), 4u);
		EXPECT_EQ(q.mission.primary[0], 3);
	}
	EXPECT_EQ(byPos.at({ 9, 5, 0})->getQuest().mission.heroLevel, 5);
	{
		const auto & q = byPos.at({10, 5, 0})->getQuest();
		ASSERT_EQ(q.mission.heroes.size(), 1u);
		EXPECT_EQ(q.mission.heroes[0], HeroTypeID(0));
	}
	{
		const auto & q = byPos.at({11, 5, 0})->getQuest();
		ASSERT_EQ(q.mission.players.size(), 1u);
		EXPECT_EQ(q.mission.players[0], PlayerColor(2));
	}
}

TEST(TinyH3MBuilderTest, SeerHutMissionAndReward)
{
	// Mission = bring 1000 gold; reward = 500 experience. Also checks the NOTHING
	// reward path on a second seer hut (LEVEL mission, NOTHING reward).
	using B = TinyH3M::TinyH3MBuilder;

	auto bytes = B(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("SeerHutReward")
		.seerHut({5, 5, 0},
			B::missionResources({0, 0, 0, 0, 0, 0, 1000}),
			B::rewardExperience(500))
		.seerHut({6, 6, 0},
			B::missionLevel(3),
			B::rewardNothing())
		.seerHut({7, 7, 0},
			B::missionLevel(4),
			B::rewardResource(GameResID(GameResID::WOOD), 25))
		.seerHut({8, 8, 0},
			B::missionLevel(2),
			B::rewardMana(20))
		.buildAndDump("SeerHutMissionAndReward");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);

	// CGQuestGuard derives from CGSeerHut, so filter the guards out.
	std::vector<const CGSeerHut *> seers;
	for(const auto & obj : loaded.map->objects)
	{
		const auto * seer = dynamic_cast<const CGSeerHut *>(obj.get());
		if(seer && dynamic_cast<const CGQuestGuard *>(obj.get()) == nullptr)
			seers.push_back(seer);
	}
	ASSERT_EQ(seers.size(), 4u);

	std::map<int3, const CGSeerHut *> byPos;
	for(const auto * s : seers)
		byPos[s->anchorPos()] = s;

	{
		const auto * seer = byPos.at({5, 5, 0});
		EXPECT_EQ(seer->getQuest().mission.resources[GameResID::GOLD], 1000);
		ASSERT_FALSE(seer->configuration.info.empty());
		EXPECT_EQ(seer->configuration.info[0].reward.heroExperience, 500);
	}
	{
		// NOTHING reward emits zero payload bytes — the visit info still gets
		// recorded but with all-default reward fields.
		const auto * seer = byPos.at({6, 6, 0});
		EXPECT_EQ(seer->getQuest().mission.heroLevel, 3);
		ASSERT_FALSE(seer->configuration.info.empty());
	}
	{
		const auto * seer = byPos.at({7, 7, 0});
		EXPECT_EQ(seer->getQuest().mission.heroLevel, 4);
		ASSERT_FALSE(seer->configuration.info.empty());
		EXPECT_EQ(seer->configuration.info[0].reward.resources[GameResID::WOOD], 25);
	}
	{
		const auto * seer = byPos.at({8, 8, 0});
		EXPECT_EQ(seer->getQuest().mission.heroLevel, 2);
		ASSERT_FALSE(seer->configuration.info.empty());
		EXPECT_EQ(seer->configuration.info[0].reward.manaDiff, 20);
	}
}

TEST(TinyH3MBuilderTest, HeroCustomisation)
{
	// Hero with garrison, experience, primary skills, and a backpack artifact.
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("HeroCustom")
		.playerActive(PlayerColor(0))
		.hero({5, 5, 0}, HeroTypeID(0), PlayerColor(0))
		.heroGarrison({{CreatureID(0), 10}, {CreatureID(1), 5}}) // 10 pikemen + 5 archers
		.heroExperience(40000)                                    // ~level 5
		.heroPrimary(5, 3, 1, 2)
		.heroBackpack({ArtifactID(7)})                            // Centaur Axe in backpack
		.buildAndDump("HeroCustomisation");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);

	const auto * hero = findFirst<CGHeroInstance>(*loaded.map);
	ASSERT_NE(hero, nullptr);
	EXPECT_EQ(hero->exp, 40000);
	// Garrison: 7 slots, first two populated.
	EXPECT_TRUE(hero->hasStackAtSlot(SlotID(0)));
	EXPECT_TRUE(hero->hasStackAtSlot(SlotID(1)));
	EXPECT_EQ(hero->getStackCount(SlotID(0)), 10);
	EXPECT_EQ(hero->getStackCount(SlotID(1)), 5);
}

TEST(TinyH3MBuilderTest, KillCreatureQuest)
{
	// Monster + quest guard that targets the monster's wire identifier.
	TinyH3M::TinyH3MBuilder b(EMapFormat::SOD);
	b.size(36).name("KillQuest")
		.playerActive(PlayerColor(0))
		.hero({5, 5, 0}, HeroTypeID(0), PlayerColor(0))
		.monster({10, 10, 0}, CreatureID(27), 3); // Gold Dragons
	const auto target = b.lastHandle();
	ASSERT_NE(target.wireIdentifier, 0u);

	b.questGuard({15, 15, 0}, TinyH3M::TinyH3MBuilder::missionKillCreature(target)
		.withLastDay(10)
		.withFirstVisitText("Slay the dragons"));

	auto loaded = loadMap(b.buildAndDump("KillCreatureQuest"));
	ASSERT_NE(loaded.map, nullptr);

	const auto * guard = findFirst<CGQuestGuard>(*loaded.map);
	ASSERT_NE(guard, nullptr);
	EXPECT_EQ(guard->getQuest().lastDay, 10);
	// Loader resolves the uint32 wire id to an ObjectInstanceID in afterRead;
	// the resolved target lives on quest.killTarget once mapping completes.
	EXPECT_TRUE(guard->getQuest().killTarget.hasValue());
}

TEST(TinyH3MBuilderTest, TwoRandomTownsSOD)
{
	// H3 editor only opens maps at the canonical sizes (S=36, M=72, L=108, XL=144);
	// 9x9 parses through CMapLoaderH3M fine but the editor refuses to open it.
	const int3 redPos { 5,  5, 0};
	const int3 bluePos{30, 30, 0};

	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("TwoTowns")
		.description("Two random towns owned by red and blue")
		.playerActive(PlayerColor(0))
		.playerActive(PlayerColor(1))
		.randomTown(redPos, PlayerColor(0))
		.randomTown(bluePos, PlayerColor(1))
		.buildAndDump("TwoRandomTownsSOD");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);
	EXPECT_EQ(loaded.map->width, 36);
	EXPECT_EQ(loaded.map->height, 36);

	const auto & townIds = loaded.map->getAllTowns();
	ASSERT_EQ(townIds.size(), 2u);

	std::map<int3, PlayerColor> seen;
	for(ObjectInstanceID id : townIds)
	{
		const auto * obj = loaded.map->getObject(id);
		ASSERT_NE(obj, nullptr);
		seen[obj->anchorPos()] = obj->getOwner();
	}

	ASSERT_TRUE(seen.count(redPos))  << "No town at " << redPos.toString();
	ASSERT_TRUE(seen.count(bluePos)) << "No town at " << bluePos.toString();
	EXPECT_EQ(seen[redPos],  PlayerColor(0));
	EXPECT_EQ(seen[bluePos], PlayerColor(1));
}
