/*
 * api/Skill.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/Skill.h>

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class SkillProxy : public RawPointerWrapper<const Skill, SkillProxy>
{
public:
	static constexpr std::string_view luaName = "Skill";

	static void registerMethods(MethodRegistrar & R);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<Skill>)
{
	return std::string(SkillProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
