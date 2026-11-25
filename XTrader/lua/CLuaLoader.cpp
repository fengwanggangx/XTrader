#include "CLuaLoader.h"

#include "defines_lua.h"
#include "../defines.h"

std::shared_ptr<CLuaLoader> CLuaLoader::Instance()
{
	static std::shared_ptr<CLuaLoader> instance(new CLuaLoader(), [](CLuaLoader* p)
	{
		delete p;
	});
	return instance;
}

CLuaLoader::CLuaLoader()
{
	if (InitEnvironment())
	{
		InitFunctionPtr();
	}
}

CLuaLoader::~CLuaLoader()
{
	func_lua_getglobal = nullptr;
	func_lua_gettable = nullptr;
	func_lua_getfield = nullptr;
	func_lua_geti = nullptr;
	func_lua_rawget = nullptr;
	func_lua_rawgeti = nullptr;
	func_lua_rawgetp = nullptr;

	func_luaL_newmetatable = nullptr;
	func_lua_pcallk = nullptr;
	func_luaL_loadfilex = nullptr;
	func_luaL_loadstring = nullptr;

	func_luaL_checklstring = nullptr;
	func_luaL_error = nullptr;

	func_lua_pushnil = nullptr;
	func_lua_pushnumber = nullptr;
	func_lua_pushinteger = nullptr;
	func_lua_pushlstring = nullptr;
	func_lua_pushstring = nullptr;
	func_lua_pushvfstring = nullptr;
	func_lua_pushfstring = nullptr;
	func_lua_pushcclosure = nullptr;
	func_lua_pushboolean = nullptr;
	func_lua_pushlightuserdata = nullptr;
	func_lua_pushthread = nullptr;

	func_lua_gc = nullptr;

	func_lua_setglobal = nullptr;
	func_lua_settable = nullptr;
	func_lua_setfield = nullptr;
	func_lua_seti = nullptr;
	func_lua_rawset = nullptr;
	func_lua_rawseti = nullptr;
	func_lua_rawsetp = nullptr;
	func_lua_setmetatable = nullptr;

	func_lua_setiuservalue = nullptr;
	func_lua_absindex = nullptr;
	func_lua_gettop = nullptr;
	func_lua_settop = nullptr;
	func_lua_pushvalue = nullptr;
	func_lua_rotate = nullptr;
	func_lua_copy = nullptr;
	func_lua_checkstack = nullptr;

	func_luaL_newstate = nullptr;
	func_lua_close = nullptr;
	func_luaL_openlibs = nullptr;
	func_lua_isinteger = nullptr;
	func_lua_isstring = nullptr;
	if (nullptr != m_handle)
	{
		FreeLibrary(m_handle);
	}
}

static constexpr const wchar_t* cs_dll = L"lua54.dll";
bool CLuaLoader::InitEnvironment()
{
	if (nullptr != m_handle)
	{
		return true;
	}
	m_handle = ::LoadLibraryEx(cs_dll, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (nullptr == m_handle)
	{
		return false;
	}
	return true;
}

void CLuaLoader::InitFunctionPtr()
{
	if (nullptr == m_handle)
	{
		return;
	}

	BIND_DLL_FUNC(func_lua_getglobal, lua_getglobal, m_handle);
	BIND_DLL_FUNC(func_lua_gettable, lua_gettable, m_handle);
	BIND_DLL_FUNC(func_lua_getfield, lua_getfield, m_handle);
	BIND_DLL_FUNC(func_lua_geti, lua_geti, m_handle);
	BIND_DLL_FUNC(func_lua_rawget, lua_rawget, m_handle);
	BIND_DLL_FUNC(func_lua_rawgeti, lua_rawgeti, m_handle);
	BIND_DLL_FUNC(func_lua_rawgetp, lua_rawgetp, m_handle);

	BIND_DLL_FUNC(func_luaL_newmetatable, luaL_newmetatable, m_handle);

	BIND_DLL_FUNC(func_lua_pcallk, lua_pcallk, m_handle);
	BIND_DLL_FUNC(func_luaL_loadfilex, luaL_loadfilex, m_handle);
	BIND_DLL_FUNC(func_luaL_loadstring, luaL_loadstring, m_handle);

	BIND_DLL_FUNC(func_lua_isnumber, lua_isnumber, m_handle);
	BIND_DLL_FUNC(func_lua_isstring, lua_isstring, m_handle);
	BIND_DLL_FUNC(func_lua_iscfunction, lua_iscfunction, m_handle);
	BIND_DLL_FUNC(func_lua_isinteger, lua_isinteger, m_handle);
	BIND_DLL_FUNC(func_lua_isuserdata, lua_isuserdata, m_handle);
	BIND_DLL_FUNC(func_lua_type, lua_type, m_handle);
	BIND_DLL_FUNC(func_lua_typename, lua_typename, m_handle);

	BIND_DLL_FUNC(func_lua_tonumberx, lua_tonumberx, m_handle);
	BIND_DLL_FUNC(func_lua_tointegerx, lua_tointegerx, m_handle);
	BIND_DLL_FUNC(func_lua_toboolean, lua_toboolean, m_handle);
	BIND_DLL_FUNC(func_lua_tolstring, lua_tolstring, m_handle);
	BIND_DLL_FUNC(func_lua_rawlen, lua_rawlen, m_handle);
	BIND_DLL_FUNC(func_lua_tocfunction, lua_tocfunction, m_handle);
	BIND_DLL_FUNC(func_lua_touserdata, lua_touserdata, m_handle);
	BIND_DLL_FUNC(func_lua_tothread, lua_tothread, m_handle);
	BIND_DLL_FUNC(func_lua_topointer, lua_topointer, m_handle);

	BIND_DLL_FUNC(func_luaL_checklstring, luaL_checklstring, m_handle);
	BIND_DLL_FUNC(func_luaL_error, luaL_error, m_handle);

	BIND_DLL_FUNC(func_lua_pushnil, lua_pushnil, m_handle);
	BIND_DLL_FUNC(func_lua_pushnumber, lua_pushnumber, m_handle);
	BIND_DLL_FUNC(func_lua_pushinteger, lua_pushinteger, m_handle);
	BIND_DLL_FUNC(func_lua_pushlstring, lua_pushlstring, m_handle);
	BIND_DLL_FUNC(func_lua_pushstring, lua_pushstring, m_handle);
	BIND_DLL_FUNC(func_lua_pushvfstring, lua_pushvfstring, m_handle);
	BIND_DLL_FUNC(func_lua_pushfstring, lua_pushfstring, m_handle);
	BIND_DLL_FUNC(func_lua_pushcclosure, lua_pushcclosure, m_handle);
	BIND_DLL_FUNC(func_lua_pushboolean, lua_pushboolean, m_handle);
	BIND_DLL_FUNC(func_lua_pushlightuserdata, lua_pushlightuserdata, m_handle);
	BIND_DLL_FUNC(func_lua_pushthread, lua_pushthread, m_handle);

	BIND_DLL_FUNC(func_lua_gc, lua_gc, m_handle);

	BIND_DLL_FUNC(func_lua_setglobal, lua_setglobal, m_handle);
	BIND_DLL_FUNC(func_lua_settable, lua_settable, m_handle);
	BIND_DLL_FUNC(func_lua_setfield, lua_setfield, m_handle);
	BIND_DLL_FUNC(func_lua_seti, lua_seti, m_handle);
	BIND_DLL_FUNC(func_lua_rawset, lua_rawset, m_handle);
	BIND_DLL_FUNC(func_lua_rawseti, lua_rawseti, m_handle);
	BIND_DLL_FUNC(func_lua_rawsetp, lua_rawsetp, m_handle);
	BIND_DLL_FUNC(func_lua_setmetatable, lua_setmetatable, m_handle);
	BIND_DLL_FUNC(func_lua_setiuservalue, lua_setiuservalue, m_handle);

	BIND_DLL_FUNC(func_lua_absindex, lua_absindex, m_handle);
	BIND_DLL_FUNC(func_lua_gettop, lua_gettop, m_handle);
	BIND_DLL_FUNC(func_lua_settop, lua_settop, m_handle);
	BIND_DLL_FUNC(func_lua_pushvalue, lua_pushvalue, m_handle);
	BIND_DLL_FUNC(func_lua_rotate, lua_rotate, m_handle);
	BIND_DLL_FUNC(func_lua_copy, lua_copy, m_handle);
	BIND_DLL_FUNC(func_lua_checkstack, lua_checkstack, m_handle);

	BIND_DLL_FUNC(func_luaL_newstate, luaL_newstate, m_handle);
	BIND_DLL_FUNC(func_lua_close, lua_close, m_handle);
	BIND_DLL_FUNC(func_luaL_openlibs, luaL_openlibs, m_handle);
	BIND_DLL_FUNC(func_lua_isinteger, lua_isinteger, m_handle);
	BIND_DLL_FUNC(func_lua_isstring, lua_isstring, m_handle);
}
