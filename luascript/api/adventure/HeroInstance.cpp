/*
 * HeroInstance.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "HeroInstance.h"

#include "../Registry.h"

#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting
{
namespace api
{
const std::vector<HeroInstanceProxy::CustomRegType> HeroInstanceProxy::REGISTER_CUSTOM =
{
	{"getStack",      LuaMethodWrapper<&CCreatureSet::getStackPtr, CGHeroInstance>::invoke, false},
	{"getOwner",      LuaMethodWrapper<&CGObjectInstance::getOwner, CGHeroInstance>::invoke, false},
	{"getNameTextID", LuaMethodWrapper<&CGHeroInstance::getNameTextID>::invoke, false},
	{"isMale", LuaFunctionWrapper<&HeroInstanceProxy::isMale>::invoke, false},
	{"isFemale", LuaFunctionWrapper<&HeroInstanceProxy::isFemale>::invoke, false},
};

bool HeroInstanceProxy::isMale(const CGHeroInstance * hero)
{
	return hero->gender == EHeroGender::MALE;
}

bool HeroInstanceProxy::isFemale(const CGHeroInstance * hero)
{
	return hero->gender == EHeroGender::FEMALE;
}

}
}

VCMI_LIB_NAMESPACE_END
