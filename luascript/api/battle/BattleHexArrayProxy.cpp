/*
 * BattleHexArrayProxy.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleHexArrayProxy.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api::battle
{

const std::vector<BattleHexArrayProxy::CustomRegType> BattleHexArrayProxy::REGISTER_CUSTOM =
{
	{"insert", LuaFunctionWrapper<&BattleHexArrayProxy::insert>::invoke, false}
};

void BattleHexArrayProxy::insert(BattleHexArray & hexes, BattleHex target)
{
	hexes.insert(target);
}

}

VCMI_LIB_NAMESPACE_END
