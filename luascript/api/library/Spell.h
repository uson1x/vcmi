/*
 * api/Spell.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/spells/Spell.h>

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class SpellProxy : public RawPointerWrapper<const ::spells::Spell, SpellProxy>
{
public:
	static constexpr std::string_view luaName = "Spell";

	using Wrapper = RawPointerWrapper<const ::spells::Spell, SpellProxy>;
	static void registerMethods(MethodRegistrar & R);

	static std::vector<std::string> getSchools(const ::spells::Spell & spell);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<::spells::Spell>)
{
	return std::string(SpellProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
