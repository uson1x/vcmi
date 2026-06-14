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
#include "../../lib/filesystem/CMemoryBuffer.h"
#include "../../lib/mapping/CMap.h"
#include "../../lib/mapping/CMapHeader.h"
#include "../../lib/mapping/MapFormat.h"
#include "../../lib/mapping/MapFormatH3M.h"

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
		.description("Phase 1 acceptance fixture")
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
		.description("Phase 2 SOD acceptance fixture")
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

TEST(TinyH3MBuilderTest, Phase4And5AllObjectTypes)
{
	// A blank SOD map carrying one instance of every object type the builder
	// supports as of phases 4+5. The acceptance bar is "CMapLoaderH3M::loadMap
	// returns a non-null CMap" — body assertions for each kind live in the
	// dedicated per-type tests added later.
	auto bytes = TinyH3M::TinyH3MBuilder(EMapFormat::SOD)
		.size(36, /*twoLevel*/ false)
		.name("AllObjects")
		.description("Phase 4+5 acceptance fixture")
		.playerActive(PlayerColor(0))
		// Phase 3
		.randomTown({5, 5, 0}, PlayerColor(0))
		// Phase 4
		.monster({10, 10, 0}, CreatureID(27), /*count*/ 7) // Gold Dragon
		.resource({11, 10, 0}, GameResID(GameResID::GOLD), /*amount*/ 1000)
		.artifact({12, 10, 0}, ArtifactID(7))              // Centaur Axe (first artifact with map sprite)
		// Phase 5
		.keymaster({15, 15, 0}, /*color*/ 0)
		.borderGuard({16, 15, 0}, /*color*/ 0)
		.borderGate ({17, 15, 0}, /*color*/ 0)
		.questGuard ({20, 20, 0})
		.seerHut    ({22, 22, 0})
		.buildAndDump("Phase4And5AllObjectTypes");

	auto loaded = loadMap(std::move(bytes));
	ASSERT_NE(loaded.map, nullptr);
	EXPECT_EQ(loaded.map->width, 36);
	// Object count: 1 town + 1 monster + 1 resource + 1 artifact + 3 border + 1 quest guard + 1 seer = 9
	size_t live = 0;
	for(const auto & obj : loaded.map->objects)
		if(obj) ++live;
	EXPECT_EQ(live, 9u);
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
		.description("Phase 3 acceptance fixture")
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
