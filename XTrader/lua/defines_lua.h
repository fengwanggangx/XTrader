#ifndef __LUA_DEFINES_H__
#define __LUA_DEFINES_H__
#include <lua.h>
#include <lauxlib.h>

using lua_getglobal_t = int (*)(lua_State* L, const char* name);
using lua_gettable_t = int (*)(lua_State* L, int idx);
using lua_getfield_t = int (*)(lua_State* L, int idx, const char* k);
using lua_geti_t = int (*)(lua_State* L, int idx, lua_Integer n);
using lua_rawget_t = int (*)(lua_State* L, int idx);
using lua_rawgeti_t = int (*)(lua_State* L, int idx, lua_Integer n);
using lua_rawgetp_t = int (*)(lua_State* L, int idx, const void* p);

using luaL_newmetatable_t = int (*)(lua_State* L, const char* tname);

using lua_pcallk_t = int (*)(lua_State* L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
using luaL_loadfilex_t = int (*)(lua_State* L, const char* filename, const char* mode);
using luaL_loadstring_t = int (*)(lua_State* L, const char* s);

using lua_isnumber_t = int (*)(lua_State* L, int idx);
using lua_iscfunction_t = int (*)(lua_State* L, int idx);
using lua_isuserdata_t = int (*)(lua_State* L, int idx);
using lua_type_t = int (*)(lua_State* L, int idx);
using lua_typename_t = const char* (*)(lua_State* L, int tp);

using lua_tonumberx_t = lua_Number (*)(lua_State* L, int idx, int* isnum);
using lua_tointegerx_t = lua_Integer (*)(lua_State* L, int idx, int* isnum);
using lua_toboolean_t = int (*)(lua_State* L, int idx);
using lua_tolstring_t = const char* (*)(lua_State* L, int idx, size_t* len);
using lua_rawlen_t = lua_Unsigned (*)(lua_State* L, int idx);
using lua_tocfunction_t = lua_CFunction (*)(lua_State* L, int idx);
using lua_touserdata_t = void* (*)(lua_State* L, int idx);
using lua_tothread_t = lua_State* (*)(lua_State* L, int idx);
using lua_topointer_t = const void* (*)(lua_State* L, int idx);

using luaL_checklstring_t = const char* (*)(lua_State* L, int idx, size_t* len);
using luaL_error_t = int (*)(lua_State* L, const char* fmt, ...);

using lua_pushnil_t = void (*)(lua_State* L);
using lua_pushnumber_t = void (*)(lua_State* L, lua_Number n);
using lua_pushinteger_t = void (*)(lua_State* L, lua_Integer n);
using lua_pushlstring_t = const char* (*)(lua_State* L, const char* s, size_t len);
using lua_pushstring_t = const char* (*)(lua_State* L, const char* s);
using lua_pushvfstring_t = const char* (*)(lua_State* L, const char* fmt, va_list argp);
using lua_pushfstring_t = const char* (*)(lua_State* L, const char* fmt, ...);
using lua_pushcclosure_t = void (*)(lua_State* L, lua_CFunction fn, int n);
using lua_pushboolean_t = void (*)(lua_State* L, int b);
using lua_pushlightuserdata_t = void (*)(lua_State* L, void* p);
using lua_pushthread_t = int (*)(lua_State* L);

using lua_gc_t = int (*)(lua_State* L, int what, ...);

using lua_setglobal_t = void (*)(lua_State* L, const char* name);
using lua_settable_t = void (*)(lua_State* L, int idx);
using lua_setfield_t = void (*)(lua_State* L, int idx, const char* k);
using lua_seti_t = void (*)(lua_State* L, int idx, lua_Integer n);
using lua_rawset_t = void (*)(lua_State* L, int idx);
using lua_rawseti_t = void (*)(lua_State* L, int idx, lua_Integer n);
using lua_rawsetp_t = void (*)(lua_State* L, int idx, const void* p);
using lua_setmetatable_t = int (*)(lua_State* L, int objindex);
using lua_setiuservalue_t = int (*)(lua_State* L, int idx, int n);

using lua_absindex_t = int (*)(lua_State* L, int idx);
using lua_gettop_t = int (*)(lua_State* L);
using lua_settop_t = void (*)(lua_State* L, int idx);
using lua_pushvalue_t = void (*)(lua_State* L, int idx);
using lua_rotate_t = void (*)(lua_State* L, int idx, int n);
using lua_copy_t = void (*)(lua_State* L, int fromidx, int toidx);
using lua_checkstack_t = int (*)(lua_State* L, int n);

using luaL_newstate_t = lua_State* (*)();
using lua_close_t = void (*)(lua_State*);
using luaL_openlibs_t = void (*)(lua_State*);
using luaL_dostring_t = int (*)(lua_State*, const char*);
using luaL_dofile_t = int (*)(lua_State*, const char*);
using lua_tostring_t = const char* (*)(lua_State*, int);
using lua_isinteger_t = int (*)(lua_State*, int);
using lua_tointeger_t = lua_Integer (*)(lua_State*, int);
using lua_isstring_t = int (*)(lua_State*, int);
using lua_pop_t = void (*)(lua_State*, int);
using lua_pcall_t = int (*)(lua_State*, int, int, int);
using lua_isfunction_t = int (*)(lua_State*, int);

extern lua_getglobal_t func_lua_getglobal;
extern lua_gettable_t func_lua_gettable;
extern lua_getfield_t func_lua_getfield;
extern lua_geti_t func_lua_geti;
extern lua_rawget_t func_lua_rawget;
extern lua_rawgeti_t func_lua_rawgeti;
extern lua_rawgetp_t func_lua_rawgetp;

extern luaL_newmetatable_t func_luaL_newmetatable;

extern lua_pcallk_t func_lua_pcallk;
extern luaL_loadfilex_t func_luaL_loadfilex;
extern luaL_loadstring_t func_luaL_loadstring;

extern lua_isnumber_t func_lua_isnumber;
extern lua_iscfunction_t func_lua_iscfunction;
extern lua_isuserdata_t func_lua_isuserdata;
extern lua_type_t func_lua_type;
extern lua_typename_t func_lua_typename;

extern lua_tonumberx_t func_lua_tonumberx;
extern lua_tointegerx_t func_lua_tointegerx;
extern lua_toboolean_t func_lua_toboolean;
extern lua_tolstring_t func_lua_tolstring;
extern lua_rawlen_t func_lua_rawlen;
extern lua_tocfunction_t func_lua_tocfunction;
extern lua_touserdata_t func_lua_touserdata;
extern lua_tothread_t func_lua_tothread;
extern lua_topointer_t func_lua_topointer;

extern luaL_checklstring_t func_luaL_checklstring;
extern luaL_error_t func_luaL_error;

extern lua_pushnil_t func_lua_pushnil;
extern lua_pushnumber_t func_lua_pushnumber;
extern lua_pushinteger_t func_lua_pushinteger;
extern lua_pushlstring_t func_lua_pushlstring;
extern lua_pushstring_t func_lua_pushstring;
extern lua_pushvfstring_t func_lua_pushvfstring;
extern lua_pushfstring_t func_lua_pushfstring;
extern lua_pushcclosure_t func_lua_pushcclosure;
extern lua_pushboolean_t func_lua_pushboolean;
extern lua_pushlightuserdata_t func_lua_pushlightuserdata;
extern lua_pushthread_t func_lua_pushthread;

extern lua_gc_t func_lua_gc;

extern lua_setglobal_t func_lua_setglobal;
extern lua_settable_t func_lua_settable;
extern lua_setfield_t func_lua_setfield;
extern lua_seti_t func_lua_seti;
extern lua_rawset_t func_lua_rawset;
extern lua_rawseti_t func_lua_rawseti;
extern lua_rawsetp_t func_lua_rawsetp;
extern lua_setmetatable_t func_lua_setmetatable;
extern lua_setiuservalue_t func_lua_setiuservalue;

extern lua_absindex_t func_lua_absindex;
extern lua_gettop_t func_lua_gettop;
extern lua_settop_t func_lua_settop;
extern lua_pushvalue_t func_lua_pushvalue;
extern lua_rotate_t func_lua_rotate;
extern lua_copy_t func_lua_copy;
extern lua_checkstack_t func_lua_checkstack;

extern luaL_newstate_t func_luaL_newstate;
extern lua_close_t func_lua_close;
extern luaL_openlibs_t func_luaL_openlibs;
extern lua_tostring_t func_lua_tostring;
extern lua_isinteger_t func_lua_isinteger;
extern lua_isstring_t func_lua_isstring;
extern lua_pop_t func_lua_pop;
extern lua_isfunction_t func_lua_isfunction;

#define func_lua_pushcfunction(L, f) func_lua_pushcclosure(L, (f), 0)
#define func_lua_register(L, n, f) (func_lua_pushcfunction(L, (f)), func_lua_setglobal(L, (n)))

#define func_luaL_loadfile(L, f) func_luaL_loadfilex(L, f, NULL)

#define func_lua_pcall(L, n, r, f) func_lua_pcallk(L, (n), (r), (f), 0, NULL)

#define func_luaL_dofile(L, fn) \
	(func_luaL_loadfile(L, fn) || func_lua_pcall(L, 0, LUA_MULTRET, 0))

#define func_lua_tostring(L, i) func_lua_tolstring(L, (i), NULL)

#define func_lua_tonumber(L, i) func_lua_tonumberx(L, (i), NULL)
#define func_lua_tointeger(L, i) func_lua_tointegerx(L, (i), NULL)

#define func_lua_pop(L, n) func_lua_settop(L, -(n) - 1)

#define func_lua_isfunction(L, n) (func_lua_type(L, (n)) == LUA_TFUNCTION)

#define func_luaL_dostring(L, s) \
	(luaL_loadstring(L, s) || func_lua_pcall(L, 0, LUA_MULTRET, 0))

#define func_lua_islightuserdata(L, n) (func_lua_type(L, (n)) == LUA_TLIGHTUSERDATA)

#define func_luaL_getmetatable(L, n) (func_lua_getfield(L, LUA_REGISTRYINDEX, (n)))
#endif