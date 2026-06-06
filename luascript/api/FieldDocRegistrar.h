/*
 * FieldDocRegistrar.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "SignatureOf.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

/// Field-shaped counterpart to DocRegistrar.
/// Doubles as a serializer for ApiSerializable types: its templated call operator matches
/// the lambda shape that `T::serializeScript(s)` invokes (`s(name, value, description)`).
/// Each call records a documentation entry; the actual value is ignored.
class FieldDocRegistrar final
{
public:
	struct Entry
	{
		std::string name;
		std::string type;
		std::string description;
	};

	template<typename Field>
	void operator()(const std::string & name, const Field &, std::string_view description)
	{
		entries.push_back({ name, luaTypeName<Field>(), std::string(description) });
	}

	const std::vector<Entry> & get() const { return entries; }

private:
	std::vector<Entry> entries;
};

}

VCMI_LIB_NAMESPACE_END
