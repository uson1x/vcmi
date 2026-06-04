/*
 * LuaMetaString.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/scripting/ApiTags.h>

VCMI_LIB_NAMESPACE_BEGIN

class MetaString;

namespace scripting::api
{

/// POD descriptor used by Lua scripts to build a MetaString.
/// `append` lists text IDs to concatenate; `replaceStrings` fills `%s`
/// placeholders in order; `replaceNumbers` fills `%d` placeholders in order.
/// `%s` and `%d` slots are independent — only order within each list matters.
struct DLL_LINKAGE LuaMetaString final : ApiSerializable<LuaMetaString>
{
	std::vector<std::string> append;
	std::vector<std::string> replaceStrings;
	std::vector<int64_t>     replaceNumbers;

	MetaString toMetaString() const;

	template<typename Serializer>
	void serializeScript(Serializer & s)
	{
		s("append",         append);
		s("replaceStrings", replaceStrings);
		s("replaceNumbers", replaceNumbers);
	}
};

}

VCMI_LIB_NAMESPACE_END
