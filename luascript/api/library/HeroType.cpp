/*
 * api/HeroType.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "HeroType.h"

#include "../Registry.h"

#include "../../LuaStack.h"
#include "../../LuaCallWrapper.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

const std::vector<HeroTypeProxy::CustomRegType> HeroTypeProxy::REGISTER_CUSTOM =
{
	{"getJsonKey",   LuaMethodWrapper<&Entity::getJsonKey, HeroType>::invoke,        false},
	{"getName",      LuaMethodWrapper<&Entity::getNameTranslated, HeroType>::invoke, false},
};

}

VCMI_LIB_NAMESPACE_END
