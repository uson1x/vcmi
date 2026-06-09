/*
 * DocRegistrar.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

/// MethodRegistrar implementation that collects bindings as metadata.
/// Drives the same `registerMethods` code path the runtime uses, but instead of pushing
/// closures onto a Lua table it accumulates {name, signature, description} entries that
/// downstream code (docs exporter, BindingsCoverageTest) can inspect.
class DocRegistrar final : public MethodRegistrar
{
public:
	struct Entry
	{
		std::string name;
		std::string signature;
		std::string description;
	};

	void addRaw(std::string_view name,
	            lua_CFunction    /*functor*/,
				const std::string & signature,
	            std::string_view description) override
	{
		entries.push_back({std::string(name), signature, std::string(description)});
	}

	const std::vector<Entry> & get() const { return entries; }

private:
	std::vector<Entry> entries;
};

}

VCMI_LIB_NAMESPACE_END
