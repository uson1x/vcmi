/*
 * LuaStack.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "api/Registry.h"
#include "../lib/constants/IdentifierBase.h"
#include "vcmi/scripting/ApiTags.h"


VCMI_LIB_NAMESPACE_BEGIN

class JsonNode;
class int3;

namespace scripting
{

class LuaApiException : public std::runtime_error
{
	using std::runtime_error::runtime_error;
};

class LuaStack;

class LuaDeserializer
{
	LuaStack & stack;
	lua_State * L;
	int idx;

public:
	LuaDeserializer(LuaStack & stack, int idx);

	template<typename T>
	void operator()(const std::string &keyName, T & data) const;
};

class LuaSerializer
{
	LuaStack & stack;
	lua_State * L;
	int idx;

public:
	LuaSerializer(LuaStack & stack, int idx);

	template<typename T>
	void operator()(const std::string &keyName, const T & data) const;
};

class LuaStack
{
public:
	LuaStack(lua_State * L_);
	void balance();
	void clear();

	void pushByIndex(lua_Integer index);

	template<typename T>
	LuaStack & operator<<(const T & parameter)
	{
		push(parameter);
		return *this;
	}

	void pushNil();
	void pushInteger(lua_Integer value);
	void push(bool value);
	void push(const char * value);
	void push(const std::string & value);
	void push(const JsonNode & value);

	template<typename T>
	void push(const std::optional<T> & value)
	{
		if(value.has_value())
			push(value.value());
		else
			pushNil();
	}

	template<typename T, typename std::enable_if_t< std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
	void push(const T value)
	{
		pushInteger(static_cast<lua_Integer>(value));
	}

	template<typename T, typename std::enable_if_t< std::is_enum_v<T>, int> = 0>
	void push(const T value)
	{
		pushInteger(static_cast<lua_Integer>(value));
	}

	void push(const int3 & value);

	template<typename T, typename std::enable_if_t< std::is_base_of_v<IdentifierBase, T>, int> = 0>
	void push(const T & value)
	{
		pushInteger(static_cast<lua_Integer>(value.getNum()));
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagRawPointer, T>, int> = 0>
	void push(T * value)
	{
		using DataType = T;
		using BaseType = DataType::ScriptingApiName;
		using PtrType = std::conditional_t<std::is_const_v<T>, const BaseType *, BaseType *>;
		static auto KEY = api::TypeRegistry::get()->getKey<PtrType>();

		if(value == nullptr)
		{
			pushNil();
			return;
		}

		void * raw = lua_newuserdata(L, sizeof(PtrType));
		if(!raw)
			throw LuaApiException("Failed to allocate new user data!");

		auto * ptr = static_cast<PtrType *>(raw);
		*ptr = value;

		luaL_getmetatable(L, KEY);
		if(lua_isnil(L, -1))
			throw LuaApiException(std::string("Unregistered type pushed on Lua stack: ") + KEY);

		lua_setmetatable(L, -2);
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagSharedPointer, T>, int> = 0>
	void push(std::shared_ptr<T> value)
	{
		using DataType = T;
		using BaseType = DataType::ScriptingApiName;
		using PtrType = std::conditional_t<std::is_const_v<T>, std::shared_ptr<const BaseType>, std::shared_ptr<BaseType>>;
		static auto KEY = api::TypeRegistry::get()->getKey<PtrType>();

		if(value == nullptr)
		{
			pushNil();
			return;
		}

		void * raw = lua_newuserdata(L, sizeof(PtrType));

		if(!raw)
			throw LuaApiException("Failed to allocate new user data!");

		new(raw) PtrType(value);

		luaL_getmetatable(L, KEY);
		if(lua_isnil(L, -1))
			throw LuaApiException(std::string("Unregistered type pushed on Lua stack: ") + KEY);

		lua_setmetatable(L, -2);
	}

	bool tryGetInteger(int position, lua_Integer & value);

	bool tryGet(int position, bool & value);

	template<typename T, typename std::enable_if_t< std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
	bool tryGet(int position, T & value)
	{
		lua_Integer temp;
		if(tryGetInteger(position, temp))
		{
			value = static_cast<T>(temp);
			return true;
		}
		else
		{
			return false;
		}
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<IdentifierBase, T>, int> = 0>
	bool tryGet(int position, T & value)
	{
		lua_Integer temp;
		if(tryGetInteger(position, temp))
		{
			value = T(temp);
			return true;
		}
		else
		{
			return false;
		}
	}

	template<typename T, typename std::enable_if_t< std::is_enum_v<T>, int> = 0>
	bool tryGet(int position, T & value)
	{
		lua_Integer temp;
		if(tryGetInteger(position, temp))
		{
			value = static_cast<T>(temp);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool tryGet(int position, int3 & value);

	bool tryGet(int position, double & value);
	bool tryGet(int position, std::string & value);

	template<typename T>
	bool tryGet(int idx, std::vector<T> & out)
	{
		if (!lua_istable(L, idx))
			throw LuaApiException("value at index is not a table");

		size_t len = lua_objlen(L, idx);
		out.resize(len);

		for (size_t i = 0; i < len; ++i) {
			lua_rawgeti(L, idx, i+1);
			tryGet(-1, out[i]);
			lua_pop(L, 1);
		}
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagSerializable, T>, int> = 0>
	STRONG_INLINE bool tryGet(int position, T & value)
	{
		LuaDeserializer serializer(*this, position);
		value->serializeLua(serializer);
		return true;
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagRawPointer, T> && std::is_const_v<T>, int> = 0>
	STRONG_INLINE bool tryGet(int position, T * & value)
	{
		using DataType = T;
		using BaseType = DataType::ScriptingApiName;
		using BasePtrType = BaseType *;
		using ConstPtrType = const BaseType *;

		ConstPtrType basePtr;
		bool result = tryGetCUData<BasePtrType, ConstPtrType>(position, basePtr);
		value = dynamic_cast<T*>(basePtr);
		return result;
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagRawPointer, T> && !std::is_const_v<T>, int> = 0>
	STRONG_INLINE bool tryGet(int position, T * & value)
	{
		using DataType = T;
		using BaseType = DataType::ScriptingApiName;
		using BasePtrType = BaseType*;

		BasePtrType basePtr;
		bool result = tryGetUData<BasePtrType>(position, basePtr);
		value = dynamic_cast<T*>(basePtr);
		return result;
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagSharedPointer, T> && std::is_const_v<T>, int> = 0>
	STRONG_INLINE bool tryGet(int position, std::shared_ptr<T> & value)
	{
		using DataType = T;
		using BaseType = DataType::ScriptingApiName;
		using BasePtrType = std::shared_ptr<BaseType>;
		using ConstPtrType = std::shared_ptr<const BaseType>;

		ConstPtrType basePtr;
		bool result = tryGetCUData<BasePtrType, ConstPtrType>(position, basePtr);
		value = std::dynamic_pointer_cast<T>(basePtr);
		return result;
	}

	template<typename T, typename std::enable_if_t<std::is_base_of_v<scripting::TagSharedPointer, T> && !std::is_const_v<T>, int> = 0>
	STRONG_INLINE bool tryGet(int position, std::shared_ptr<T> & value)
	{
		using DataType = T;
		using BaseType = DataType::ScriptingApiName;
		using BasePtrType = std::shared_ptr<BaseType>;

		BasePtrType basePtr;
		bool result = tryGetUData<BasePtrType>(position, basePtr);
		value = std::dynamic_pointer_cast<T>(basePtr);
		return result;
	}

	template<typename... Args>
	bool tryGetAll(int position, Args &...args) {
		bool failed = false;
		int i = 0;

		((tryGet(position + i, args) ? ++i : (failed = true, ++i, false)), ...);

		return !failed;
	}

	template<typename T>
	void getOrThrow(int position, T & value)
	{
		bool result = tryGet(position, value);
		if (!result)
		{
			const char * expectedType = typeid(T).name();
			const char * actualType = lua_typename(L, position);
			std::string message	= std::string("Invalid Lua value! Expected ") + expectedType + "at position" + std::to_string(position) + ", but found " + actualType;
			throw LuaApiException( message );
		}
	}

	template<typename BaseType>
	bool tryGetUData(int position, BaseType & value)
	{
		if (lua_isnil(L, position))
		{
			value = nullptr;
			return true;
		}

		static auto KEY = api::TypeRegistry::get()->getKey<BaseType>();

		void * raw = lua_touserdata(L, position);

		if(!raw)
		{
			const char * expectedType = typeid(BaseType).name();
			const char * actualType = lua_typename(L, position);
			std::string message	= std::string("Invalid Lua value! Expected ") + expectedType + "at position" + std::to_string(position) + ", but found " + actualType;
			throw LuaApiException( message );

		}

		if(lua_getmetatable(L, position) == 0)
			throw LuaApiException( "Invalid Lua value! Found userdata without assigned metatable!");

		lua_getfield(L, LUA_REGISTRYINDEX, KEY);

		if(lua_rawequal(L, -1, -2) == 1)
		{
			value = *(static_cast<BaseType *>(raw));
			lua_pop(L, 2);
			return true;
		}

		lua_pop(L, 2);
		throw LuaApiException( "Failed to retrieve class " + std::string(typeid(BaseType).name()) + " from stack!");
	}

	template<typename BaseType, typename BaseConstType>
	bool tryGetCUData(int position, BaseConstType & value)
	{
		if (lua_isnil(L, position))
		{
			value = nullptr;
			return true;
		}

		static auto KEY = api::TypeRegistry::get()->getKey<BaseType>();
		static auto C_KEY = api::TypeRegistry::get()->getKey<BaseConstType>();

		void * raw = lua_touserdata(L, position);

		if(!raw)
		{
			const char * expectedType = typeid(BaseType).name();
			const char * actualType = lua_typename(L, position);
			std::string message	= std::string("Invalid Lua value! Expected ") + expectedType + "at position" + std::to_string(position) + ", but found " + actualType;
			throw LuaApiException( message );
		}

		if(lua_getmetatable(L, position) == 0)
			throw LuaApiException( "Invalid Lua value! Found userdata without assigned metatable!");

		//top is metatable

		lua_getfield(L, LUA_REGISTRYINDEX, KEY);

		if(lua_rawequal(L, -1, -2) == 1)
		{
			value = *(static_cast<BaseType *>(raw));
			lua_pop(L, 2);
			return true;
		}

		lua_pop(L, 1);

		//top is metatable

		lua_getfield(L, LUA_REGISTRYINDEX, C_KEY);

		if(lua_rawequal(L, -1, -2) == 1)
		{
			value = *(static_cast<BaseConstType *>(raw));
			lua_pop(L, 2);
			return true;
		}

		lua_pop(L, 2);
		throw LuaApiException( "Failed to retrieve class " + std::string(typeid(BaseType).name()) + " from stack!");
	}

	bool tryGet(int position, JsonNode & value);

	int retNil();
	int retVoid();

	STRONG_INLINE
	int retPushed()
	{
		return lua_gettop(L);
	}

	inline bool isFunction(int position)
	{
		return lua_isfunction(L, position);
	}

	inline bool isNumber(int position)
	{
		return lua_isnumber(L, position);
	}

	static int quickRetBool(lua_State * L, bool value)
	{
		lua_settop(L, 0);
		lua_pushboolean(L, value);
		return 1;
	}

	template<typename T>
	static int quickRetInt(lua_State * L, const T & value)
	{
		lua_settop(L, 0);
		lua_pushinteger(L, static_cast<int32_t>(value));
		return 1;
	}

	template<std::size_t T>
	static int quickRetInt(lua_State * L, const std::bitset<T> & value)
	{
		lua_settop(L, 0);
		lua_pushinteger(L, static_cast<int32_t>(value.to_ulong()));
		return 1;
	}

	static int quickRetStr(lua_State * L, const std::string & value)
	{
		lua_settop(L, 0);
		lua_pushlstring(L, value.c_str(), value.size());
		return 1;
	}

private:
	lua_State * L;
	int initialTop;
};

template<typename T>
void LuaDeserializer::operator()(const std::string &keyName, T & data) const
{
	if (!lua_istable(L, idx))
		throw LuaApiException("value at index is not a table");

	// pushes table[keyName] on stack
	lua_getfield(L, idx, keyName.c_str());

	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		throw LuaApiException("Missing required field '" + keyName + "'");
	}

	try {
		stack.tryGet(-1, data);
	} catch (...) {
		// restore stack
		lua_pop(L, 1);
		throw;
	}

	lua_pop(L, 1); // pop pushed value
}

template<typename T>
void LuaSerializer::operator()(const std::string &keyName, const T & data) const
{
	if (!lua_istable(L, idx))
		throw LuaApiException("value at index is not a table");

    stack.push(data);
    lua_setfield(L, idx, keyName.c_str());
}

}

VCMI_LIB_NAMESPACE_END
