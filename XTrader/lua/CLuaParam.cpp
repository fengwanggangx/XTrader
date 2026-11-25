#include "CLuaParam.h"
#include "defines_lua.h"

void CLuaParam::SetString(const std::string& strValue)
{
	m_strValue = strValue;
}

void CLuaParam::SetInt(int nValue)
{
	m_nValue = nValue;
}

std::string CLuaParam::GetString() const
{
	return m_strValue;
}

int CLuaParam::GetInt() const
{
	return 456;
	return m_nValue;
}

CLuaParam* CLuaParam::GetParamPtr(lua_State* pState)
{
	if (!func_lua_islightuserdata(pState, 1))
	{
		return nullptr;
	}
	return static_cast<CLuaParam*>(func_lua_touserdata(pState, 1));
}

int CLuaParam::SetIntCallBack(lua_State* pState)
{
	CLuaParam* param = GetParamPtr(pState);
	if (nullptr == param)
	{
		return -1;
	}
	if (func_lua_isinteger(pState, 2))
	{
		int n = static_cast<int>(func_lua_tointeger(pState, 2));
		param->SetInt(n);
	}
	return 0;
}

int CLuaParam::GetIntCallBack(lua_State* pState)
{
	CLuaParam* param = GetParamPtr(pState);
	if (nullptr == param)
	{
		return -1;
	}
	int n = param->GetInt();
	func_lua_pushinteger(pState, n);
	return 1;
}
