/*
 * BonusDescriptor.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/ApiTags.h>

#include "../../../lib/json/JsonNode.h"

#include "../SignatureOf.h"

VCMI_LIB_NAMESPACE_BEGIN

struct Bonus;

namespace scripting::api
{

/// POD descriptor for a Bonus configuration passed from Lua scripts.
struct BonusDescriptor final : ApiSerializable<BonusDescriptor>
{
	static constexpr std::string_view luaName = "BonusDescriptor";
	static constexpr std::string_view luaDescription =
		"Configuration for a Bonus that scripts hand to ServerCallback `addUnitBonus` or "
		"`addBattleBonus`. Fields mirror the JSON bonus schema (see config/schemas/bonus.json) "
		"— the host materialises a Bonus through JsonUtils::parseBonus.";

	si32 val = 0;
	si32 turns = 0;
	bool hidden = false;
	std::string stacking;
	std::string description;
	std::string icon;

	JsonNode type;
	JsonNode subtype;
	JsonNode valueType;
	JsonNode effectRange;
	JsonNode duration;
	JsonNode sourceType;
	JsonNode sourceID;
	JsonNode targetSourceType;
	JsonNode addInfo;
	JsonNode limiters;
	JsonNode propagator;
	JsonNode updater;
	JsonNode propagationUpdater;

	/// Builds a JsonNode matching the schema JsonUtils::parseBonus expects.
	JsonNode toJson() const;

	/// Materializes a Bonus by routing through JsonUtils::parseBonus.
	Bonus toBonus() const;

	template<typename Serializer>
	void serializeScript(Serializer & s)
	{
		s("val",                val,                "Numeric magnitude of the bonus (interpreted per `valueType`).");
		s("turns",              turns,              "Remaining duration in turns; relevant for the N_TURNS / N_DAYS durations.");
		s("hidden",             hidden,             "If true, the bonus is not shown in the unit's bonus list UI.");
		s("stacking",           stacking,           "Stacking key — bonuses sharing it overwrite rather than accumulate.");
		s("description",        description,        "Optional human-readable description (used by tooltips when shown).");
		s("icon",               icon,               "Optional icon name shown next to the bonus in the UI.");

		s("type",               type,               "Bonus type name (e.g. \"PRIMARY_SKILL\", \"FIRE_IMMUNITY\"). See BonusType enum.");
		s("subtype",            subtype,            "Sub-selector that narrows the type (e.g. specific creature, school, primary stat).");
		s("valueType",          valueType,          "How `val` is combined with other bonuses: additive, base, percent-of-..., independent min/max.");
		s("effectRange",        effectRange,        "Spatial scope the bonus applies in (e.g. only on native terrain).");
		s("duration",           duration,           "When the bonus expires — permanent, one-battle, n-turns, until-attacked, etc.");
		s("sourceType",         sourceType,         "Origin class (artifact, spell effect, secondary skill, …) — drives source-based dispels.");
		s("sourceID",           sourceID,           "Identifier of the specific source within its sourceType.");
		s("targetSourceType",   targetSourceType,   "Source type the bonus is restricted to act upon (used by anti-magic / specialty bonuses).");
		s("addInfo",            addInfo,            "Optional auxiliary payload — meaning depends on the bonus type.");
		s("limiters",           limiters,           "JSON-defined limiter chain that gates whether the bonus applies to a given bearer.");
		s("propagator",         propagator,         "Rule for sharing the bonus with neighbouring entities (army-wide, hex-wide, …).");
		s("updater",            updater,            "Per-turn or per-event recalculation of `val` (e.g. growing with stack count).");
		s("propagationUpdater", propagationUpdater, "Updater applied to bonuses produced by this one's propagator.");
	}
};

inline std::string luaTypeNameOf(LuaTypeNameTag<BonusDescriptor>)
{
	return std::string(BonusDescriptor::luaName);
}

/// JsonNode fields on BonusDescriptor accept any Lua value (string, number, table, …) and
/// are funneled through JsonUtils::parseBonus. Surface that openness rather than "userdata".
inline std::string luaTypeNameOf(LuaTypeNameTag<JsonNode>)
{
	return "any";
}

}

VCMI_LIB_NAMESPACE_END
