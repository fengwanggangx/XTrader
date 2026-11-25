#ifndef __CLUAVM_H__
#define __CLUAVM_H__

#include <memory>
#include <string>

class CLuaParam;
class CLuaLoader;
struct lua_State;
class CLuaVM final
{
public:
	CLuaVM();
	~CLuaVM();

public:
	int Execute(lua_State* pState, int nType);
	int Execute(int nType, CLuaParam* pParam);
	bool LoadScript(const std::string& strPath);

private:
	void Regist();

private:
	lua_State* m_pState{ nullptr };
	std::shared_ptr<CLuaLoader> m_loader{ nullptr };
};

#endif
