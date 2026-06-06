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
		s("val",                val);
		s("turns",              turns);
		s("hidden",             hidden);
		s("stacking",           stacking);
		s("description",        description);
		s("icon",               icon);

		s("type",               type);
		s("subtype",            subtype);
		s("valueType",          valueType);
		s("effectRange",        effectRange);
		s("duration",           duration);
		s("sourceType",         sourceType);
		s("sourceID",           sourceID);
		s("targetSourceType",   targetSourceType);
		s("addInfo",            addInfo);
		s("limiters",           limiters);
		s("propagator",         propagator);
		s("updater",            updater);
		s("propagationUpdater", propagationUpdater);
	}
};

inline std::string luaTypeNameOf(LuaTypeNameTag<BonusDescriptor>)
{
	return "BonusDescriptor";
}

}

VCMI_LIB_NAMESPACE_END
