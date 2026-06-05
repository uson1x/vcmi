/*
 * SignatureOf.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../LuaCallWrapper.h"
#include "../../lib/constants/IdentifierBase.h"
#include "../../include/vcmi/scripting/ApiTags.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace scripting::api
{

/// Tag struct used as an ADL hook for mapping a C++ type to its Lua-facing type name.
/// Proxies provide an inline overload `luaTypeNameOf(LuaTypeNameTag<TheirCppType>)` in this
/// namespace; ADL on this tag finds them. The primary template below covers primitives,
/// integers, enums, identifiers, optionals, vectors and falls back to "userdata".
template<class T>
struct LuaTypeNameTag {};

template<class T>
inline std::string luaTypeNameOf(LuaTypeNameTag<T>)
{
	using U = std::remove_cvref_t<T>;
	if constexpr (std::is_same_v<U, bool>)
		return "boolean";
	else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, const char *>)
		return "string";
	else if constexpr (std::is_floating_point_v<U>)
		return "number";
	else if constexpr (std::is_integral_v<U>)
		return "integer";
	else if constexpr (std::is_enum_v<U>)
		return "integer";
	else if constexpr (std::is_base_of_v<IdentifierBase, U>)
		return "integer";
	else
		return "userdata";
}

template<class T>
inline std::string luaTypeNameOf(LuaTypeNameTag<std::vector<T>>)
{
	return luaTypeNameOf(LuaTypeNameTag<std::remove_cvref_t<T>>{}) + "[]";
}

template<class T>
inline std::string luaTypeNameOf(LuaTypeNameTag<std::optional<T>>)
{
	return luaTypeNameOf(LuaTypeNameTag<std::remove_cvref_t<T>>{}) + "?";
}

/// Public entry point — strips cv/reference qualifiers, then dispatches via ADL.
template<class T>
inline std::string luaTypeName()
{
	return luaTypeNameOf(LuaTypeNameTag<std::remove_cvref_t<T>>{});
}

namespace detail
{
template<class Tuple, std::size_t Offset, std::size_t... I>
inline std::string formatArgsImpl(std::index_sequence<I...>)
{
	std::string out;
	const char * sep = "";
	((out += std::exchange(sep, ", "), out += luaTypeName<std::tuple_element_t<I + Offset, Tuple>>()), ...);
	return out;
}

template<class Tuple, std::size_t Offset = 0>
inline std::string formatArgs()
{
	static_assert(std::tuple_size_v<Tuple> >= Offset, "Offset exceeds tuple size");
	constexpr std::size_t N = std::tuple_size_v<Tuple> - Offset;
	return formatArgsImpl<Tuple, Offset>(std::make_index_sequence<N>{});
}

template<class Ret>
inline std::string formatReturn()
{
	if constexpr (std::is_void_v<Ret>)
		return {};
	else
		return ": " + luaTypeName<Ret>();
}
}

/// Produces a Lua-style signature for a member function pointer, e.g. "(a: integer): boolean".
template<auto Method>
inline std::string signatureOfMethod()
{
	using TR = LuaClassMemberTraits<decltype(Method)>;
	return "(" + detail::formatArgs<typename TR::ArgsTuple>() + ")" + detail::formatReturn<typename TR::ReturnType>();
}

/// Produces a Lua-style signature for a static proxy method registered through LuaFunctionWrapper.
/// The first C++ argument is the self/object value and is dropped — Lua sees it as `self`.
template<auto Fn>
inline std::string signatureOfFunction()
{
	using TR = LuaFunctionTraits<std::remove_pointer_t<std::decay_t<decltype(Fn)>>>;
	using Args = typename TR::ArgsTuple;
	static_assert(std::tuple_size_v<Args> >= 1, "Proxy static helpers must take the object as their first argument");
	return "(" + detail::formatArgs<Args, /*Offset=*/1>() + ")" + detail::formatReturn<typename TR::ReturnType>();
}

}

VCMI_LIB_NAMESPACE_END
