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

// Returned pair keeps the backing buffer alive for the lifetime of the CMap.
std::pair<std::unique_ptr<CMap>, std::unique_ptr<CMemoryBuffer>> loadMap(std::vector<uint8_t> bytes)
{
	auto buf = std::make_unique<CMemoryBuffer>();
	buf->write(bytes.data(), static_cast<si64>(bytes.size()));
	buf->seek(0);

	CMapLoaderH3M loader("TinyH3MBuilderTest", "core", "ASCII", buf.get());
	auto map = loader.loadMap(nullptr);
	return {std::move(map), std::move(buf)};
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

	auto [map, buf] = loadMap(std::move(bytes));
	ASSERT_NE(map, nullptr);
	EXPECT_EQ(map->width, 36);
	EXPECT_EQ(map->height, 36);
	EXPECT_EQ(map->levels(), 1);
	EXPECT_TRUE(map->getHeroesOnMap().empty());
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

	auto [map, buf] = loadMap(std::move(bytes));
	ASSERT_NE(map, nullptr);
	EXPECT_EQ(map->width, 36);
	EXPECT_EQ(map->height, 36);
	EXPECT_EQ(map->levels(), 1);
	EXPECT_TRUE(map->getHeroesOnMap().empty());
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

	auto [map, buf] = loadMap(std::move(bytes));
	ASSERT_NE(map, nullptr);
	EXPECT_EQ(map->width, 36);
	EXPECT_EQ(map->height, 36);

	const auto & townIds = map->getAllTowns();
	ASSERT_EQ(townIds.size(), 2u);

	std::map<int3, PlayerColor> seen;
	for(ObjectInstanceID id : townIds)
	{
		const auto * obj = map->getObject(id);
		ASSERT_NE(obj, nullptr);
		seen[obj->anchorPos()] = obj->getOwner();
	}

	ASSERT_TRUE(seen.count(redPos))  << "No town at " << redPos.toString();
	ASSERT_TRUE(seen.count(bluePos)) << "No town at " << bluePos.toString();
	EXPECT_EQ(seen[redPos],  PlayerColor(0));
	EXPECT_EQ(seen[bluePos], PlayerColor(1));
}
