/*
 * api/HeroClass.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/HeroClass.h>

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class HeroClassProxy : public RawPointerWrapper<const HeroClass, HeroClassProxy>
{
public:
	static constexpr std::string_view luaName = "HeroClass";

	static void registerMethods(MethodRegistrar & R);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<HeroClass>)
{
	return std::string(HeroClassProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
