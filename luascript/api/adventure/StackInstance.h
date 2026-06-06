/*
 * StackInstance.h, part of VCMI engine
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

#include "../../../lib/mapObjects/army/CStackInstance.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class StackInstanceProxy : public RawPointerWrapper<const CStackInstance, StackInstanceProxy>
{
public:
	static constexpr std::string_view luaName = "StackInstance";

	using Wrapper = RawPointerWrapper<const CStackInstance, StackInstanceProxy>;
	static void registerMethods(MethodRegistrar & R);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<CStackInstance>)
{
	return std::string(StackInstanceProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
