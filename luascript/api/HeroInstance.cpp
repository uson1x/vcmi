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

#include "Registry.h"

#include "../LuaStack.h"
#include "../LuaCallWrapper.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting
{
namespace api
{
VCMI_REGISTER_CORE_SCRIPT_API(HeroInstanceProxy, "adventure.HeroInstance");

const std::vector<HeroInstanceProxy::CustomRegType> HeroInstanceProxy::REGISTER_CUSTOM =
{
	{"getStack", LuaMethodWrapper<CGHeroInstance, decltype(&CCreatureSet::getStackPtr), &CCreatureSet::getStackPtr>::invoke, false},
	{"getOwner", LuaMethodWrapper<CGHeroInstance, decltype(&CGObjectInstance::getOwner), &CGObjectInstance::getOwner>::invoke, false},
	{"getNameTextID", LuaMethodWrapper<CGHeroInstance, decltype(&CGHeroInstance::getNameTextID), &CGHeroInstance::getNameTextID>::invoke, false},
	{"isMale", &HeroInstanceProxy::isMale, false},
	{"isFemale", &HeroInstanceProxy::isFemale, false},
};

int HeroInstanceProxy::isMale(lua_State * L)
{
	LuaStack S(L);
	const CGHeroInstance * object;
	if(!S.tryGet(1, object))
		return S.retNil();
	return LuaStack::quickRetBool(L, object->gender == EHeroGender::MALE);
}

int HeroInstanceProxy::isFemale(lua_State * L)
{
	LuaStack S(L);
	const CGHeroInstance * object;
	if(!S.tryGet(1, object))
		return S.retNil();
	return LuaStack::quickRetBool(L, object->gender == EHeroGender::FEMALE);
}

}
}

VCMI_LIB_NAMESPACE_END
