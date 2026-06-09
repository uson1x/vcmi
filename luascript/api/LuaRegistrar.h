/*
 * LuaRegistrar.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "MethodRegistrar.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

/// MethodRegistrar implementation that pushes each binding directly onto a Lua table.
/// Construct with the lua_State and the absolute stack index of the table that should
/// receive the methods; `addRaw` then writes name -> C-function pairs into that table.
class LuaRegistrar final : public MethodRegistrar
{
public:
	LuaRegistrar(lua_State * L_, int targetTableIdx)
		: L(L_), targetIdx(targetTableIdx)
	{}

	void addRaw(std::string_view name,
	            lua_CFunction    functor,
	            std::string      /*signature*/,
	            std::string_view /*description*/) override
	{
		lua_pushlstring(L, name.data(), name.size());
		lua_pushcclosure(L, functor, 0);
		lua_rawset(L, targetIdx);
	}

private:
	lua_State * L;
	int         targetIdx;
};

}

VCMI_LIB_NAMESPACE_END
