/*$I0 */

/* File: rumble_lua.h Author: Administrator Created on January 30, 2011, 5:15 AM */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#ifndef RUMBLE_LUA_H
#   define RUMBLE_LUA_H
#   ifdef __cplusplus
extern "C"
{
#   endif
signed int  rumble_lua_callback(lua_State *state, void *hook, void *session);
int         Foo_register(lua_State *L);
#   ifdef __cplusplus
}
#   endif
#endif /* RUMBLE_LUA_H */
