#include "CLuaVM.h"
#include "CLuaLoader.h"
#include "defines_lua.h"
#include <stdexcept>
#include "CLuaParam.h"

constexpr const char* cs_entry_point = "main";

int CExecute(lua_State* pState)
{
	func_lua_getglobal(pState, "vm_instance");
	CLuaVM* pVm = static_cast<CLuaVM*>(func_lua_touserdata(pState, -1));
	func_lua_pop(pState, 1);
	if (nullptr == pVm)
	{
		func_luaL_error(pState, "Invalid CLuaVM instance");
		return -1;
	}
	int bNumber = 0;
	lua_Integer luaType = func_lua_tointegerx(pState, 1, &bNumber);
	if (!bNumber)
	{
		return -1;
	}
	int nType = static_cast<int>(luaType);
	int nRet = pVm->Execute(pState, nType);
	func_lua_pushinteger(pState, nRet);
	return 1;
}

int CPrint(lua_State* pState)
{
	std::size_t sz = 0;
	const char* p = func_lua_tolstring(pState, -1, &sz);
	if (nullptr == p)
	{
		return -1;
	}
	std::string str(p, sz);
	return 0;
}

CLuaVM::CLuaVM()
{
	m_loader = CLuaLoader::Instance();
	if (nullptr != func_luaL_newstate)
	{
		m_pState = func_luaL_newstate();
	}
	if (nullptr != func_luaL_openlibs)
	{
		func_luaL_openlibs(m_pState);
	}
	Regist();
}

CLuaVM::~CLuaVM()
{
	if (nullptr != m_pState)
	{
		if (nullptr != func_lua_close)
		{
			func_lua_close(m_pState);
		}
	}
}

bool CLuaVM::LoadScript(const std::string& strPath)
{
	if (nullptr == m_pState)
	{
		return false;
	}

	int nRet = func_luaL_dofile(m_pState, strPath.c_str());
	if (LUA_OK != nRet)
	{
		const char* cc = func_lua_tostring(m_pState, -1);
		func_lua_pop(m_pState, 1);
		return false;
	}
	func_lua_getglobal(m_pState, cs_entry_point);
	bool bWithEntryPoint = func_lua_isfunction(m_pState, -1);
	func_lua_pop(m_pState, 1);
	if (!bWithEntryPoint)
	{
		return false;
	}

	return true;
}

int CLuaVM::Execute(int nType, CLuaParam* pParam)
{
	int nRet = -1;
	if (nullptr == m_pState)
	{
		return nRet;
	}

	if (nType == -1)
	{
		return func_lua_gc(m_pState, LUA_GCCOLLECT, 0);
	}

	func_lua_settop(m_pState, 0);
	func_lua_getglobal(m_pState, cs_entry_point);
	if (!func_lua_isfunction(m_pState, -1))
	{
		func_lua_pop(m_pState, 1);
		return nRet;
	}

	func_lua_pushinteger(m_pState, nType);
	if (nullptr != pParam)
	{
		func_lua_pushlightuserdata(m_pState, pParam);
	}
	else
	{
		func_lua_pushnil(m_pState);
	}

	func_luaL_getmetatable(m_pState, "CLuaParam");
	func_lua_setmetatable(m_pState, -2);

	static constexpr int cs_nArgs = 2;
	int ret = func_lua_pcall(m_pState, cs_nArgs, LUA_MULTRET, 0);
	if (LUA_OK != ret)
	{
		size_t str_len;
		const char* cc = func_luaL_checklstring(m_pState, 1, &str_len);
		func_lua_pop(m_pState, 1);
		return nRet;
	}
	int nReturned = func_lua_gettop(m_pState);
	if (nReturned >= 1 && func_lua_isinteger(m_pState, -1))
	{
		lua_Integer ret = func_lua_tointeger(m_pState, -1);
		if ((ret < INT_MIN) || (ret > INT_MAX))
		{
			nRet = -1;
			throw std::runtime_error("CLuaVM::Execute overflow return value");
		}
		nRet = static_cast<int>(ret);
	}
	else
	{
		nRet = -1;
		throw std::runtime_error("CLuaVM::Execute Unknow return value");
	}
	func_lua_pop(m_pState, 1);
	return nRet;
}

void CLuaVM::Regist()
{
	func_lua_pushlightuserdata(m_pState, this);
	func_lua_setglobal(m_pState, "vm_instance");
	func_lua_register(m_pState, "CExecute", CExecute);
	func_lua_register(m_pState, "CPrint", CPrint);

	func_luaL_newmetatable(m_pState, "CLuaParam");
	func_lua_pushvalue(m_pState, -1);
	func_lua_setfield(m_pState, -2, "__index");

	func_lua_pushcfunction(m_pState, &CLuaParam::GetIntCallBack);
	func_lua_setfield(m_pState, -2, "GetInt");

	func_lua_pushcfunction(m_pState, &CLuaParam::SetIntCallBack);
	func_lua_setfield(m_pState, -2, "SetInt");

	func_lua_pop(m_pState, 1);
}

int CLuaVM::Execute(lua_State* pState, int nType)
{
	return 789;
}
