/*
 * TinyH3MBuilder.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../lib/GameConstants.h"
#include "../../lib/int3.h"
#include "../../lib/mapping/MapFormat.h"
#include "../../lib/mapping/MapDifficulty.h"

namespace TinyH3M
{

class TinyH3MWriter;

/// Fluent API for building .h3m binary streams in test code.
///
/// Builder holds *intent-level* state (Spec structs). At build() time, per-section
/// emitters walk the state and produce bytes via TinyH3MWriter, consulting
/// MapFormatFeaturesH3M for per-format field sizes.
///
/// Phase 1 scope: a parseable header that round-trips through
/// CMapLoaderH3M::loadMapHeader. Players default to "neither human nor computer"
/// (i.e., 13-byte inactive blocks for SOD). No objects, no terrain emitter,
/// no map options — those land in later phases.
class TinyH3MBuilder
{
public:
	explicit TinyH3MBuilder(EMapFormat format = EMapFormat::ROE);

	// ---- fluent setters -------------------------------------------------

	TinyH3MBuilder & size(int sideLength, bool twoLevel = false);
	TinyH3MBuilder & name(std::string s);
	TinyH3MBuilder & description(std::string s);
	TinyH3MBuilder & difficulty(EMapDifficulty d);

	/// Mark a player as both human- and computer-playable. Required for any
	/// object that ownership-validates against canAnyonePlay (towns, heroes).
	TinyH3MBuilder & playerActive(PlayerColor color);

	/// Append a Random Town object owned by `owner`. Builder auto-registers the
	/// RANDOM_TOWN template on first call. No garrison, standard fort, no events.
	TinyH3MBuilder & randomTown(const int3 & pos, PlayerColor owner);

	// ---- Phase 4: wanderer objects -------------------------------------

	/// Monster stack on the adventure map. `character` is 0..4 (compliant..savage).
	TinyH3MBuilder & monster(const int3 & pos, CreatureID creature, uint16_t count, int8_t character = 2);

	/// Resource pile. amount=0 means "use default".
	TinyH3MBuilder & resource(const int3 & pos, GameResID resource, uint32_t amount = 0);

	/// Specific artifact pickup.
	TinyH3MBuilder & artifact(const int3 & pos, ArtifactID artifact);

	// ---- Phase 5: quest objects ----------------------------------------

	/// Keymaster Tent. Subid encodes the keymaster colour (0..7).
	TinyH3MBuilder & keymaster(const int3 & pos, int color);

	/// Border Guard checking for the matching keymaster colour.
	TinyH3MBuilder & borderGuard(const int3 & pos, int color);

	/// Border Gate (pathfinder-passable variant) checking for the matching keymaster colour.
	TinyH3MBuilder & borderGate(const int3 & pos, int color);

	/// Quest Guard with NONE mission (always accepts). Mission customisation TBD.
	TinyH3MBuilder & questGuard(const int3 & pos);

	/// Seer Hut with NONE mission (always accepts). Mission/reward customisation TBD.
	TinyH3MBuilder & seerHut(const int3 & pos);

	// ---- output ---------------------------------------------------------

	/// Emit the uncompressed .h3m byte stream. The output covers everything
	/// CMapLoaderH3M::loadMapHeader needs to parse and nothing else.
	std::vector<uint8_t> build();

	/// Build the uncompressed bytes, gzip them, and drop the resulting .h3m
	/// into VCMIDirs::get().userCachePath() / "testMaps" / (testName + ".h3m").
	/// Returns the same uncompressed bytes (so callers can immediately feed
	/// them to CMapLoaderH3M without re-decompressing). Intended to be called
	/// from every .h3m-producing test so the fixtures can be opened in the
	/// original H3 / HOTA editor for manual verification.
	std::vector<uint8_t> buildAndDump(const std::string & testName);

private:
	struct ObjectSpec
	{
		MapObjectID    id;
		MapObjectSubID subid;
		int3           position;
		PlayerColor    owner = PlayerColor::NEUTRAL;
		uint32_t       templateIndex = 0; // resolved at build() time

		// Body-specific payload. Only the fields appropriate for `id` are read.
		uint16_t       monsterCount      = 0;
		int8_t         monsterCharacter  = 0;
		uint32_t       resourceAmount    = 0;
	};

	// Internal helper: register a (id, subid) template in the table if not already there;
	// return its index. The actual byte content is sourced from Data/Objects.txt at emit time.
	uint32_t registerTemplate(MapObjectID id, MapObjectSubID subid);

	void writeHeader(TinyH3MWriter & w) const;
	void writePlayerInfo(TinyH3MWriter & w) const;
	void writeStandardVictoryLoss(TinyH3MWriter & w) const;
	void writeTeamInfo(TinyH3MWriter & w) const;
	void writeAllowedHeroes(TinyH3MWriter & w) const;
	void writeDisposedHeroes(TinyH3MWriter & w) const;
	void writeMapOptions(TinyH3MWriter & w) const;
	void writeAllowedArtifacts(TinyH3MWriter & w) const;
	void writeAllowedSpellsAbilities(TinyH3MWriter & w) const;
	void writeRumors(TinyH3MWriter & w) const;
	void writePredefinedHeroes(TinyH3MWriter & w) const;
	void writeTerrain(TinyH3MWriter & w) const;
	void writeObjectTemplates(TinyH3MWriter & w) const;
	void writeObjects(TinyH3MWriter & w) const;
	void writeEvents(TinyH3MWriter & w) const;

	EMapFormat     format;
	int            sideLength = 36;
	bool           twoLevel = false;
	std::string    mapName = "TinyH3M test map";
	std::string    mapDescription;
	EMapDifficulty mapDifficulty = EMapDifficulty::NORMAL;

	std::array<bool, 8> playerEnabled{};
	std::vector<std::pair<MapObjectID, MapObjectSubID>> templates;
	std::vector<ObjectSpec>                              objects;
};

} // namespace TinyH3M
