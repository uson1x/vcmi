/*
 * MethodRegistrar.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../LuaCallWrapper.h"
#include "SignatureOf.h"

// Pulls in the ADL overloads of `luaTypeNameOf` for engine enums (BattleSide, EWallPart, …).
// Without this, a proxy .cpp that registers a method whose signature uses one of those enums
// instantiates the generic `luaTypeName<T>` template before the overload is visible — the
// linker then prefers that fallback symbol and the signature appears as bare `integer`.
#include "Enums.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

/// Visitor that consumes per-proxy method bindings.
/// Each proxy implements `static void registerMethods(MethodRegistrar &)` and calls
/// the templated helpers on this interface. Concrete implementations either push the
/// bindings directly onto a Lua metatable (LuaRegistrar) or collect them as metadata
/// for documentation export (DocRegistrar).
class MethodRegistrar
{
public:
	virtual ~MethodRegistrar() = default;

	/// Lowest-level entry; the templated helpers below all funnel through this.
	virtual void addRaw(std::string_view name,
	                    lua_CFunction    functor,
						const std::string & signature,
	                    std::string_view description) = 0;

	/// Register a C++ member function. Signature is auto-derived from the method pointer type.
	/// `ExplicitObjectType` is required only when the method is defined on an untagged base class.
	template<auto Method, class ExplicitObjectType = void>
	void method(std::string_view name, std::string_view description)
	{
		using W = LuaMethodWrapper<Method, ExplicitObjectType>;
		addRaw(name, &W::invoke, signatureOfMethod<Method>(), description);
	}

	/// Register a static proxy helper that takes the object as its first argument.
	/// Signature is auto-derived; the first C++ argument is treated as self and dropped.
	template<auto Fn>
	void function(std::string_view name, std::string_view description)
	{
		using W = LuaFunctionWrapper<Fn>;
		addRaw(name, &W::invoke, signatureOfFunction<Fn>(), description);
	}

	/// Register a raw `int(lua_State *)` function. There is nothing to auto-derive from such a
	/// signature, so the caller must spell out the Lua-facing signature explicitly.
	template<auto Fn>
	void cfunction(std::string_view name,
				   const std::string & signature,
	               std::string_view description)
	{
		addRaw(name, &LuaCallWrapper<Fn>::invoke, signature, description);
	}
};

}

VCMI_LIB_NAMESPACE_END
