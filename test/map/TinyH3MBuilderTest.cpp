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
