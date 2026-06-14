/*
 * TinyH3MWriter.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "TinyH3MWriter.h"

#include "../../lib/int3.h"

namespace TinyH3M
{

TinyH3MWriter::TinyH3MWriter() = default;

void TinyH3MWriter::setFormatLevel(const MapFormatFeaturesH3M & newFeatures)
{
	features = newFeatures;
}

void TinyH3MWriter::setIdentifierRemapper(const MapIdentifiersH3M & newRemapper)
{
	remapper = newRemapper;
}

// ---- primitives --------------------------------------------------------

// Host endianness matches stream endianness on little-endian hosts (the only
// build target the reader supports without byte-swapping at write time too).
// If we ever need to support a big-endian host, mirror CBinaryReader::readLE
// here with a writeLE helper.

void TinyH3MWriter::writeUInt8(uint8_t v)
{
	data.push_back(v);
}

void TinyH3MWriter::writeInt8(int8_t v)
{
	data.push_back(static_cast<uint8_t>(v));
}

void TinyH3MWriter::writeUInt16(uint16_t v)
{
	data.push_back(static_cast<uint8_t>(v & 0xff));
	data.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}

void TinyH3MWriter::writeInt16(int16_t v)
{
	writeUInt16(static_cast<uint16_t>(v));
}

void TinyH3MWriter::writeUInt32(uint32_t v)
{
	data.push_back(static_cast<uint8_t>(v & 0xff));
	data.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
	data.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
	data.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

void TinyH3MWriter::writeInt32(int32_t v)
{
	writeUInt32(static_cast<uint32_t>(v));
}

void TinyH3MWriter::writeBool(bool v)
{
	data.push_back(v ? 1 : 0);
}

void TinyH3MWriter::writeBaseString(const std::string & s)
{
	writeUInt32(static_cast<uint32_t>(s.size()));
	data.insert(data.end(), s.begin(), s.end());
}

// ---- skip helpers ------------------------------------------------------

void TinyH3MWriter::skipUnused(size_t amount)
{
	data.insert(data.end(), amount, 0);
}

void TinyH3MWriter::skipZero(size_t amount)
{
	data.insert(data.end(), amount, 0);
}

// ---- identifiers -------------------------------------------------------

void TinyH3MWriter::writeInt3(const int3 & p)
{
	writeUInt8(static_cast<uint8_t>(p.x));
	writeUInt8(static_cast<uint8_t>(p.y));
	writeUInt8(static_cast<uint8_t>(p.z));
}

void TinyH3MWriter::writeHero(HeroTypeID v)
{
	// MapReaderH3M::readHero -> readUInt8; sentinel = features.heroIdentifierInvalid
	if(v.getNum() < 0)
		writeUInt8(static_cast<uint8_t>(features.heroIdentifierInvalid));
	else
		writeUInt8(static_cast<uint8_t>(v.getNum()));
}

void TinyH3MWriter::writeHeroPortrait(HeroTypeID v)
{
	if(v == HeroTypeID::NONE || v.getNum() < 0)
		writeUInt8(static_cast<uint8_t>(features.heroIdentifierInvalid));
	else
		writeUInt8(static_cast<uint8_t>(v.getNum()));
}

void TinyH3MWriter::writeArtifact(ArtifactID v)
{
	const bool wide = features.levelAB;
	const uint32_t invalid = features.artifactIdentifierInvalid;
	const uint32_t raw = (v == ArtifactID::NONE) ? invalid : static_cast<uint32_t>(v.getNum());

	if(wide)
		writeUInt16(static_cast<uint16_t>(raw));
	else
		writeUInt8(static_cast<uint8_t>(raw));
}

void TinyH3MWriter::writeCreature(CreatureID v)
{
	const bool wide = features.levelAB;
	const uint32_t invalid = features.creatureIdentifierInvalid;
	const uint32_t raw = (v == CreatureID::NONE) ? invalid : static_cast<uint32_t>(v.getNum());

	if(wide)
		writeUInt16(static_cast<uint16_t>(raw));
	else
		writeUInt8(static_cast<uint8_t>(raw));
}

void TinyH3MWriter::writePlayer(PlayerColor v)
{
	if(v == PlayerColor::NEUTRAL || v.getNum() < 0)
		writeUInt8(0xff);
	else
		writeUInt8(static_cast<uint8_t>(v.getNum()));
}

void TinyH3MWriter::writeTerrain(TerrainId v)
{
	writeUInt8(static_cast<uint8_t>(v.getNum()));
}

void TinyH3MWriter::writeRoad(RoadId v)
{
	writeInt8(static_cast<int8_t>(v.getNum()));
}

void TinyH3MWriter::writeRiver(RiverId v)
{
	writeInt8(static_cast<int8_t>(v.getNum()));
}

void TinyH3MWriter::writeSpell(SpellID v)
{
	if(v == SpellID::NONE || v.getNum() < 0)
		writeUInt8(static_cast<uint8_t>(features.spellIdentifierInvalid));
	else
		writeUInt8(static_cast<uint8_t>(v.getNum()));
}

void TinyH3MWriter::writeSpell32(SpellID v)
{
	if(v == SpellID::NONE || v.getNum() < 0)
		writeInt32(static_cast<int32_t>(features.spellIdentifierInvalid));
	else
		writeInt32(v.getNum());
}

void TinyH3MWriter::writeGameResID(GameResID v)
{
	writeInt8(static_cast<int8_t>(v.getNum()));
}

// ---- bitmasks ----------------------------------------------------------

template<class Identifier>
void TinyH3MWriter::writeBitmask(const std::set<Identifier> & src, int bytesToWrite, int objectsTotal, bool invert)
{
	for(int byte = 0; byte < bytesToWrite; ++byte)
	{
		uint8_t mask = 0;
		for(int bit = 0; bit < 8; ++bit)
		{
			const int index = byte * 8 + bit;
			if(index >= objectsTotal)
				break;

			const bool present = src.count(Identifier(index)) != 0;
			const bool flag = (present != invert);
			if(flag)
				mask |= static_cast<uint8_t>(1 << bit);
		}
		writeUInt8(mask);
	}
}

// Explicit instantiations for the identifier types actually used.
template void TinyH3MWriter::writeBitmask<PlayerColor>(const std::set<PlayerColor>&, int, int, bool);
template void TinyH3MWriter::writeBitmask<FactionID>(const std::set<FactionID>&, int, int, bool);
template void TinyH3MWriter::writeBitmask<HeroTypeID>(const std::set<HeroTypeID>&, int, int, bool);
template void TinyH3MWriter::writeBitmask<ArtifactID>(const std::set<ArtifactID>&, int, int, bool);
template void TinyH3MWriter::writeBitmask<SpellID>(const std::set<SpellID>&, int, int, bool);
template void TinyH3MWriter::writeBitmask<SecondarySkill>(const std::set<SecondarySkill>&, int, int, bool);

void TinyH3MWriter::writeBitmaskPlayers(const std::set<PlayerColor> & src, bool invert)
{
	writeBitmask(src, 1, 8, invert);
}

void TinyH3MWriter::writeBitmaskFactions(const std::set<FactionID> & src, bool invert)
{
	writeBitmask(src, features.factionsBytes, features.factionsCount, invert);
}

void TinyH3MWriter::writeBitmaskHeroes(const std::set<HeroTypeID> & src, bool invert)
{
	writeBitmask(src, features.heroesBytes, features.heroesCount, invert);
}

void TinyH3MWriter::writeBitmaskHeroesSized(const std::set<HeroTypeID> & src, bool invert)
{
	const uint32_t count = features.heroesCount;
	const uint32_t bytes = (count + 7) / 8;
	writeUInt32(count);
	writeBitmask(src, bytes, count, invert);
}

void TinyH3MWriter::writeBitmaskArtifacts(const std::set<ArtifactID> & src, bool invert)
{
	writeBitmask(src, features.artifactsBytes, features.artifactsCount, invert);
}

void TinyH3MWriter::writeBitmaskSpells(const std::set<SpellID> & src, bool invert)
{
	writeBitmask(src, features.spellsBytes, features.spellsCount, invert);
}

void TinyH3MWriter::writeBitmaskSkills(const std::set<SecondarySkill> & src, bool invert)
{
	writeBitmask(src, features.skillsBytes, features.skillsCount, invert);
}

void TinyH3MWriter::writeAllOnes(size_t bytes)
{
	data.insert(data.end(), bytes, 0xff);
}

} // namespace TinyH3M
