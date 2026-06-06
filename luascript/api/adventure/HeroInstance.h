/*
 * HeroInstance.h, part of VCMI engine
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

#include "../../../lib/mapObjects/CGHeroInstance.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class HeroInstanceProxy : public RawPointerWrapper<const CGHeroInstance, HeroInstanceProxy>
{
	static bool isMale(const CGHeroInstance & hero);
	static bool isFemale(const CGHeroInstance & hero);

public:
	static constexpr std::string_view luaName = "HeroInstance";

	using Wrapper = RawPointerWrapper<const CGHeroInstance, HeroInstanceProxy>;
	static void registerMethods(MethodRegistrar & R);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<CGHeroInstance>)
{
	return std::string(HeroInstanceProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
