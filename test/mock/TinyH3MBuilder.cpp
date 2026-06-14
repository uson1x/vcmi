/*
 * TinyH3MBuilder.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "TinyH3MBuilder.h"

#include "TinyH3MWriter.h"

#include "../../lib/GameLibrary.h"
#include "../../lib/VCMIDirs.h"
#include "../../lib/entities/hero/CHeroHandler.h"
#include "../../lib/mapping/MapFeaturesH3M.h"
#include "../../lib/mapping/MapFormatSettings.h"

#include <zlib.h>

namespace TinyH3M
{

TinyH3MBuilder::TinyH3MBuilder(EMapFormat format_)
	: format(format_)
{
}

TinyH3MBuilder & TinyH3MBuilder::size(int sideLength_, bool twoLevel_)
{
	sideLength = sideLength_;
	twoLevel = twoLevel_;
	return *this;
}

TinyH3MBuilder & TinyH3MBuilder::name(std::string s)
{
	mapName = std::move(s);
	return *this;
}

TinyH3MBuilder & TinyH3MBuilder::description(std::string s)
{
	mapDescription = std::move(s);
	return *this;
}

TinyH3MBuilder & TinyH3MBuilder::difficulty(EMapDifficulty d)
{
	mapDifficulty = d;
	return *this;
}

std::vector<uint8_t> TinyH3MBuilder::build()
{
	TinyH3MWriter w;
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);
	w.setFormatLevel(features);
	w.setIdentifierRemapper(LIBRARY->mapFormat->getMapping(format));

	writeHeader(w);
	writeMapOptions(w);
	// readHotaScripts is HOTA9+ only and the builder doesn't emit HOTA yet, so it has no counterpart.
	writeAllowedArtifacts(w);
	writeAllowedSpellsAbilities(w);
	writeRumors(w);
	writePredefinedHeroes(w);
	writeTerrain(w);
	writeObjectTemplates(w);
	writeObjects(w);
	writeEvents(w);

	return w.take();
}

std::vector<uint8_t> TinyH3MBuilder::buildAndDump(const std::string & testName)
{
	auto bytes = build();

	const auto dir = VCMIDirs::get().userCachePath() / "testMaps";
	boost::filesystem::create_directories(dir);
	const auto path = dir / (testName + ".h3m");

	// Real .h3m files on disk are gzip-wrapped. zlib's gzFile API writes that
	// wrapping directly; no need to roll our own deflateInit2(windowBits=31).
	gzFile gz = gzopen(path.string().c_str(), "wb");
	if(gz == nullptr)
		throw std::runtime_error("TinyH3MBuilder: failed to open " + path.string() + " for writing");

	const int written = gzwrite(gz, bytes.data(), static_cast<unsigned int>(bytes.size()));
	gzclose(gz);

	if(written != static_cast<int>(bytes.size()))
		throw std::runtime_error("TinyH3MBuilder: short gzwrite to " + path.string());

	return bytes;
}

void TinyH3MBuilder::writeHeader(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// readHeader sequence (MapFormatH3M.cpp:106) — non-HOTA branch only for phase 1.
	w.writeUInt32(static_cast<uint32_t>(format));   // EMapFormat byte read as uint32

	// areAnyPlayers must be false when no human/computer can play any color, otherwise
	// the H3 editor rejects the map. Phase 1 has no active players yet, so always false.
	w.writeBool(/*areAnyPlayers*/ false);
	w.writeInt32(sideLength);                       // height = width
	w.writeBool(twoLevel);
	w.writeBaseString(mapName);
	w.writeBaseString(mapDescription);
	w.writeUInt8(static_cast<uint8_t>(mapDifficulty));

	if(features.levelAB)
		w.writeUInt8(/*levelLimit*/ 0);

	writePlayerInfo(w);
	writeStandardVictoryLoss(w);
	writeTeamInfo(w);
	writeAllowedHeroes(w);
	writeDisposedHeroes(w);
}

void TinyH3MBuilder::writePlayerInfo(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// All 8 players default to "neither human nor computer".
	//
	// The loader takes a `skipUnused` shortcut for inactive players, but the H3
	// editor still parses the bytes against the active-player schema (so it can
	// remember "what would happen if this player were activated"). We must write
	// canonical inactive values, otherwise the editor refuses to open the map.
	//
	// Wire layout for an active player at its minimum size (no main town, no
	// custom hero), matching MapFormatH3M.cpp:240:
	//   aiTactic              uint8        (0 = AI_RANDOM)
	//   factionSelectable     uint8 (SOD)  (0)
	//   allowedFactions       bitmask      (factionsBytes; 0 = none)
	//   isFactionRandom       bool         (0)
	//   hasMainTown           bool         (0)
	//   hasRandomHero         bool         (0)
	//   mainCustomHeroId      uint8        (heroIdentifierInvalid = NONE)
	//   skipUnused            uint8 (AB+)  (0)
	//   placeholdersCount     uint32 (AB+) (0)
	//
	// Byte totals: ROE=6, AB=12, SOD=13 — match the loader's skip path exactly.
	for(int i = 0; i < PlayerColor::PLAYER_LIMIT_I; ++i)
	{
		w.writeBool(false); // canHumanPlay
		w.writeBool(false); // canComputerPlay

		w.writeUInt8(0);                                // aiTactic = AI_RANDOM
		if(features.levelSOD)
			w.writeUInt8(0);                            // SOD: faction-selectable flag
		w.skipZero(features.factionsBytes);             // allowedFactions bitmask (none)
		w.writeBool(false);                             // isFactionRandom
		w.writeBool(false);                             // hasMainTown
		w.writeBool(false);                             // hasRandomHero
		w.writeUInt8(static_cast<uint8_t>(features.heroIdentifierInvalid)); // mainCustomHeroId = NONE
		if(features.levelAB)
		{
			w.writeUInt8(0);                            // AB: unknown skip byte
			w.writeUInt32(0);                           // AB: placeholders count
		}
	}
}

void TinyH3MBuilder::writeStandardVictoryLoss(TinyH3MWriter & w) const
{
	// Standard win/loss = -1 sentinel byte each; no payload follows.
	w.writeInt8(-1); // EVictoryConditionType::WINSTANDARD
	w.writeInt8(-1); // ELossConditionType::LOSSSTANDARD
}

void TinyH3MBuilder::writeTeamInfo(TinyH3MWriter & w) const
{
	// readTeamInfo (MapFormatH3M.cpp:667): if howManyTeams == 0 no per-player table follows.
	w.writeUInt8(0);
}

void TinyH3MBuilder::writeAllowedHeroes(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// readAllowedHeroes (MapFormatH3M.cpp:685):
	//   non-HOTA: readBitmaskHeroes  (features.heroesBytes bytes)
	//   HOTA:     readBitmaskHeroesSized (uint32 count + ceil(count/8) bytes)
	// Phase 1 supports non-HOTA formats only.
	if(features.levelHOTA0)
	{
		w.writeUInt32(static_cast<uint32_t>(features.heroesCount));
		w.writeAllOnes((features.heroesCount + 7) / 8);
	}
	else
	{
		w.writeAllOnes(features.heroesBytes);
	}

	if(features.levelAB)
	{
		// placeholdersQty + per-entry readHero (1 byte each). 0 placeholders.
		w.writeUInt32(0);
	}
}

void TinyH3MBuilder::writeDisposedHeroes(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// readDisposedHeroes (MapFormatH3M.cpp:706): SOD+ only, single count byte.
	if(features.levelSOD)
		w.writeUInt8(0);
}

void TinyH3MBuilder::writeMapOptions(TinyH3MWriter & w) const
{
	// readMapOptions (MapFormatH3M.cpp:723): 31 reserved zero bytes, then HOTA-only extensions.
	// Builder does not emit HOTA yet, so just the reserved block.
	w.skipZero(31);
}

void TinyH3MBuilder::writeAllowedArtifacts(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// readAllowedArtifacts (MapFormatH3M.cpp:1466):
	//   ROE: no bytes (not AB+).
	//   AB / SOD: bitmask of features.artifactsBytes (non-HOTA).
	//   HOTA0+: sized bitmask. (Not emitted yet.)
	// invert=true on the read side, so all-zero bytes here means "default-allowed
	// set untouched" -> all standard artifacts remain allowed.
	if(features.levelAB)
		w.skipZero(features.artifactsBytes);
}

void TinyH3MBuilder::writeAllowedSpellsAbilities(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// readAllowedSpellsAbilities (MapFormatH3M.cpp:1508): SOD+ only.
	// Same invert=true convention -> zero bytes means default-allowed.
	if(features.levelSOD)
	{
		w.skipZero(features.spellsBytes);
		w.skipZero(features.skillsBytes);
	}
}

void TinyH3MBuilder::writeRumors(TinyH3MWriter & w) const
{
	// readRumors (MapFormatH3M.cpp:1520): uint32 count, then per-entry name + text.
	w.writeUInt32(0);
}

void TinyH3MBuilder::writePredefinedHeroes(TinyH3MWriter & w) const
{
	auto features = MapFormatFeaturesH3M::find(format, /*hotaVersion*/ 0);

	// readPredefinedHeroes (MapFormatH3M.cpp:1534):
	//   non-SOD: return early.
	//   SOD: loop features.heroesCount times, reading one bool 'custom' per hero.
	//   HOTA0+: prefix with uint32 heroesCount (not emitted yet).
	//   HOTA5+: trailing per-hero block (not emitted yet).
	// All-zero "custom" bools => no per-hero overrides.
	if(features.levelSOD)
		w.skipZero(features.heroesCount);
}

void TinyH3MBuilder::writeTerrain(TinyH3MWriter & w) const
{
	// readTerrain (MapFormatH3M.cpp:1699): for each tile (z,y,x), 7 bytes:
	//   terrainType, terView, riverType, riverDir, roadType, roadDir, extTileFlags.
	//
	// H3M raw terrain byte uses the H3 ordering DIRT=0, SAND=1, GRASS=2, ...
	// (which happens to match VCMI's TerrainId enum). Writing 0 would mean DIRT;
	// for a blank grass map we need 2.
	//
	// terView = 0 maps to an "edge of biome" tile in the H3 editor, which looks
	// wrong though it loads fine. Use view 0x31 (49) — a known plain-grass tile
	// from H3's tileset. Real maps put pseudo-random view bytes here.
	const int levels = twoLevel ? 2 : 1;
	const size_t tiles = static_cast<size_t>(sideLength) * sideLength * levels;
	for(size_t i = 0; i < tiles; ++i)
	{
		w.writeUInt8(2);    // terrain type = GRASS in H3M ordering
		w.writeUInt8(0x31); // terView — plain-grass tile
		w.writeUInt8(0);    // river type
		w.writeUInt8(0);    // river dir
		w.writeUInt8(0);    // road type
		w.writeUInt8(0);    // road dir
		w.writeUInt8(0);    // extTileFlags
	}
}

void TinyH3MBuilder::writeObjectTemplates(TinyH3MWriter & w) const
{
	// readObjectTemplates (MapFormatH3M.cpp:1726): uint32 count + per-template body.
	w.writeUInt32(0);
}

void TinyH3MBuilder::writeObjects(TinyH3MWriter & w) const
{
	// readObjects (MapFormatH3M.cpp:2898): uint32 count + per-object body.
	w.writeUInt32(0);
}

void TinyH3MBuilder::writeEvents(TinyH3MWriter & w) const
{
	// readEvents (MapFormatH3M.cpp:3689): uint32 count + per-event body.
	w.writeUInt32(0);
}

} // namespace TinyH3M
