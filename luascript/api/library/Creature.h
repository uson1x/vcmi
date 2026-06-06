/*
 * api/Creature.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/Creature.h>

#include "../../LuaWrapper.h"
#include "../MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

class CreatureProxy : public RawPointerWrapper<const Creature, CreatureProxy>
{
public:
	static constexpr std::string_view luaName = "Creature";

	using Wrapper = RawPointerWrapper<const Creature, CreatureProxy>;
	static void registerMethods(MethodRegistrar & R);

	static std::string getNameTextID(const Creature & creature, int amount);
};

inline std::string luaTypeNameOf(LuaTypeNameTag<Creature>)
{
	return std::string(CreatureProxy::luaName);
}

}

VCMI_LIB_NAMESPACE_END
