/*
 * TinyH3MWriter.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../lib/GameConstants.h"
#include "../../lib/mapping/MapFeaturesH3M.h"
#include "../../lib/mapping/MapIdentifiersH3M.h"

class int3;
class ObjectTemplate;
class CGObjectInstance;

namespace TinyH3M
{

/// Low-level byte writer; one-to-one mirror of MapReaderH3M.
/// Holds the same MapFormatFeaturesH3M / MapIdentifiersH3M so per-format
/// field sizes come from a single source of truth shared with the loader.
class TinyH3MWriter
{
public:
	TinyH3MWriter();

	void setFormatLevel(const MapFormatFeaturesH3M & features);
	void setIdentifierRemapper(const MapIdentifiersH3M & remapper);

	const std::vector<uint8_t> & buffer() const { return data; }
	std::vector<uint8_t>         take() { return std::move(data); }

	// ---- primitives -----------------------------------------------------

	void writeUInt8(uint8_t v);
	void writeInt8(int8_t v);
	void writeUInt16(uint16_t v);
	void writeInt16(int16_t v);
	void writeUInt32(uint32_t v);
	void writeInt32(int32_t v);
	void writeBool(bool v);

	void writeBaseString(const std::string & s);

	// ---- skip helpers ---------------------------------------------------
	// Both emit zero bytes — symmetric to MapReaderH3M::skipUnused / skipZero.

	void skipUnused(size_t amount);
	void skipZero(size_t amount);

	// ---- identifiers ----------------------------------------------------

	void writeInt3(const int3 & p);
	void writeHero(HeroTypeID v);
	void writeHeroPortrait(HeroTypeID v);
	void writeArtifact(ArtifactID v);
	void writeCreature(CreatureID v);
	void writePlayer(PlayerColor v);
	void writeTerrain(TerrainId v);
	void writeRoad(RoadId v);
	void writeRiver(RiverId v);


	// ---- bitmasks -------------------------------------------------------

	template<class Identifier>
	void writeBitmask(const std::set<Identifier> & src, int bytesToWrite, int objectsTotal, bool invert);

	void writeBitmaskPlayers(const std::set<PlayerColor> & src, bool invert);
	void writeBitmaskFactions(const std::set<FactionID> & src, bool invert);
	void writeBitmaskHeroes(const std::set<HeroTypeID> & src, bool invert);
	void writeBitmaskHeroesSized(const std::set<HeroTypeID> & src, bool invert);
	void writeBitmaskArtifacts(const std::set<ArtifactID> & src, bool invert);
	void writeBitmaskSpells(const std::set<SpellID> & src, bool invert);
	void writeBitmaskSkills(const std::set<SecondarySkill> & src, bool invert);

	// Writes `bytes` bytes whose value is 0xff (all bits set).
	// Helper for default "all-allowed" tables where building a std::set with every id is wasteful.
	void writeAllOnes(size_t bytes);

private:
	std::vector<uint8_t>     data;
	MapFormatFeaturesH3M     features;
	MapIdentifiersH3M        remapper;
};

} // namespace TinyH3M
