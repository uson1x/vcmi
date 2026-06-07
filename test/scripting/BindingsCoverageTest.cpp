/*
 * BindingsCoverageTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#if __has_include(<lua.hpp>)
#  include <lua.hpp>
#else
#  include <lua.h>
#  include <lauxlib.h>
#  include <lualib.h>
#endif

#include "../../luascript/api/Registry.h"
#include "../../luascript/api/DocRegistrar.h"
#include "../../luascript/api/FieldDocRegistrar.h"

#include <set>

namespace test
{
using ::scripting::api::Registry;
using ::scripting::api::DocRegistrar;
using ::scripting::api::FieldDocRegistrar;

TEST(BindingsCoverageTest, EveryRegisteredTypeHasBindings)
{
	const auto * registry = Registry::get();
	const auto & types = registry->getAllTypes();

	ASSERT_FALSE(types.empty()) << "Registry must contain at least one type";

	for(const auto & [typeName, registar] : types)
	{
		DocRegistrar methodSink;
		registar->collectDocs(methodSink);

		FieldDocRegistrar fieldSink;
		registar->collectFields(fieldSink);

		EXPECT_FALSE(methodSink.get().empty() && fieldSink.get().empty() && fieldSink.getEnumGroups().empty())
			<< "Type '" << typeName << "' exposes neither methods, fields, nor enum groups";
	}
}

TEST(BindingsCoverageTest, EveryEnumKeyHasNameAndDescription)
{
	const auto * registry = Registry::get();

	for(const auto & [typeName, registar] : registry->getAllTypes())
	{
		FieldDocRegistrar sink;
		registar->collectFields(sink);

		for(const auto & group : sink.getEnumGroups())
		{
			EXPECT_FALSE(group.name.empty())
				<< "Empty enum group name in type '" << typeName << "'";
			EXPECT_FALSE(group.description.empty())
				<< "Empty description for enum group '" << typeName << "." << group.name << "'";
			EXPECT_FALSE(group.keys.empty())
				<< "Enum group '" << typeName << "." << group.name << "' has no keys";

			for(const auto & key : group.keys)
			{
				EXPECT_FALSE(key.key.empty())
					<< "Empty key name in enum group '" << typeName << "." << group.name << "'";
				EXPECT_FALSE(key.description.empty())
					<< "Empty description for '" << typeName << "." << group.name << "." << key.key << "'";
			}
		}
	}
}

TEST(BindingsCoverageTest, EveryFieldHasNameTypeAndDescription)
{
	const auto * registry = Registry::get();

	for(const auto & [typeName, registar] : registry->getAllTypes())
	{
		FieldDocRegistrar sink;
		registar->collectFields(sink);

		for(const auto & entry : sink.get())
		{
			EXPECT_FALSE(entry.name.empty())
				<< "Empty field name in type '" << typeName << "'";
			EXPECT_FALSE(entry.type.empty())
				<< "Empty field type for '" << typeName << "." << entry.name << "'";
			EXPECT_FALSE(entry.description.empty())
				<< "Empty description for '" << typeName << "." << entry.name << "'";
		}
	}
}

TEST(BindingsCoverageTest, EveryBindingHasNameSignatureAndDescription)
{
	const auto * registry = Registry::get();

	for(const auto & [typeName, registar] : registry->getAllTypes())
	{
		DocRegistrar sink;
		registar->collectDocs(sink);

		for(const auto & entry : sink.get())
		{
			EXPECT_FALSE(entry.name.empty())
				<< "Empty binding name in type '" << typeName << "'";
			EXPECT_FALSE(entry.signature.empty())
				<< "Empty signature for '" << typeName << "." << entry.name << "'";
			EXPECT_FALSE(entry.description.empty())
				<< "Empty description for '" << typeName << "." << entry.name << "'";
		}
	}
}

TEST(BindingsCoverageTest, EveryRegisteredTypeHasDescription)
{
	const auto * registry = Registry::get();

	for(const auto & [typeName, registar] : registry->getAllTypes())
	{
		EXPECT_FALSE(registar->getDescription().empty())
			<< "Type '" << typeName << "' is missing a luaDescription";
	}
}

TEST(BindingsCoverageTest, BindingNamesAreUniquePerType)
{
	const auto * registry = Registry::get();

	for(const auto & [typeName, registar] : registry->getAllTypes())
	{
		DocRegistrar sink;
		registar->collectDocs(sink);

		std::set<std::string> seen;
		for(const auto & entry : sink.get())
		{
			const auto [_, inserted] = seen.insert(entry.name);
			EXPECT_TRUE(inserted)
				<< "Duplicate binding '" << entry.name << "' in type '" << typeName << "'";
		}
	}
}

}
