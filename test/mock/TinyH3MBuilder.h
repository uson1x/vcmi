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
};

} // namespace TinyH3M
