#ifndef __CLUAPARAM_H__
#define __CLUAPARAM_H__

#include <string>
struct lua_State;
class CLuaParam
{
public:
	static CLuaParam* GetParamPtr(lua_State* pState);
	static int SetIntCallBack(lua_State* pState);
	static int GetIntCallBack(lua_State* pState);

	void SetString(const std::string& strValue);
	void SetInt(int nValue);

	std::string GetString() const;
	int GetInt() const;

private:
	std::string m_strValue;
	int m_nValue{ -1 };
	double m_fValue{ 0.00 };
	bool m_bValue{ false };
};

#endif
