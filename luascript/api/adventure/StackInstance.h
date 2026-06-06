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
	static constexpr std::string_view luaDescription =
		"A creature stack as it exists in an army outside of combat (hero slot, town garrison, "
		"or map dweller). Carries the creature type, current count, and any bonuses the slot "
		"contributes. For the battle-side view of the same stack, see Unit.";

	static void registerMethods(MethodRegistrar & R);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<CStackInstance>)
{
	return std::string(StackInstanceProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
