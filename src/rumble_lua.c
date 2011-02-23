/*$I0 */
#include "rumble.h"
#ifdef RUMBLE_LUA
extern masterHandle *rumble_database_master_handle;
#   define FOO "Rumble"
typedef struct Rumble
{
    int             x;
    int             y;
    masterHandle    *m;
} rumble_lua_userdata;

/*$4
 ***********************************************************************************************************************
    SessionHandle object
 ***********************************************************************************************************************
 */

typedef struct
{
    sessionHandle   *session;
} rumble_lua_session;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static rumble_lua_session *rumble_lua_session_get(lua_State *L, int index) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_session  *bar = (rumble_lua_session *) lua_touserdata(L, index);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, index, LUA_TUSERDATA);
    if (bar == NULL) luaL_typeerror(L, index, FOO);
    return (bar);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static rumble_lua_session *rumble_lua_session_create(lua_State *L, sessionHandle *session) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_session  *bar = (rumble_lua_session *) lua_newuserdata(L, sizeof(rumble_lua_session));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    bar->session = session;

    /*
     * luaL_getmetatable(L, FOO);
     * * lua_setmetatable(L, -2);
     */
    return (bar);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_hook_on_accept(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    hookHandle      *hook = (hookHandle *) malloc(sizeof(hookHandle));
    char            svcName[32];
    rumbleService   *svc;
    cvector         *svchooks = 0;
    int             len;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    hook->func = 0;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TSTRING);
    memset(svcName, 0, 32);
    strncpy(svcName, lua_tostring(L, 2), 31);
    rumble_string_lower(svcName);
    hook->flags |= RUMBLE_HOOK_ACCEPT;
    if (!strcmp(svcName, "smtp")) svchooks = rumble_database_master_handle->smtp.init_hooks;
    if (!strcmp(svcName, "pop3")) svchooks = rumble_database_master_handle->pop3.init_hooks;
    if (!strcmp(svcName, "imap")) svchooks = rumble_database_master_handle->imap.init_hooks;
    if (!svchooks) {
        luaL_error(L, "Argument 2 <%s> isn't a known service", svcName);
        return (0);
    }

    lua_pop(L, 1);
    hook->lua_callback = luaL_ref(L, LUA_REGISTRYINDEX);
    printf("Adding Lua hook for %s service from registry #%u\n", svcName, hook->lua_callback);
    hook->flags = RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_SMTP;
    cvector_add(svchooks, hook);

    /* rumble_database_master_handle->imap. */
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_lua_callback(lua_State *L, void *hook, sessionHandle *session) {
    printf("Issuing callback for Lua::%i\n", ((hookHandle *) hook)->lua_callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ((hookHandle *) hook)->lua_callback);
    rumble_lua_session_create(L, session);
    lua_pushnumber(L, 4);
    lua_call(L, 2, 0);
    printf("Done!\n");
    return (RUMBLE_RETURN_OKAY);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static rumble_lua_userdata *toFoo(lua_State *L, int index) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar = (rumble_lua_userdata *) lua_touserdata(L, index);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * if (bar == NULL) luaL_typerror(L, index, FOO);
     */
    return (bar);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static rumble_lua_userdata *checkFoo(lua_State *L, int index) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, index, LUA_TUSERDATA);
    bar = (rumble_lua_userdata *) luaL_checkudata(L, index, FOO);

    /*
     * if (bar == NULL) luaL_typerror(L, index, FOO);
     */
    return (bar);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static rumble_lua_userdata *pushFoo(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar = (rumble_lua_userdata *) lua_newuserdata(L, sizeof(rumble_lua_userdata));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_getmetatable(L, FOO);
    lua_setmetatable(L, -2);
    return (bar);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_new(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                 x = luaL_optint(L, 1, 0);
    int                 y = luaL_optint(L, 2, 0);
    rumble_lua_userdata *bar = pushFoo(L);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    bar->x = x;
    bar->y = y;
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_yourCfunction(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar = checkFoo(L, 1);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("this is yourCfunction\t");
    lua_pushnumber(L, bar->x);
    lua_pushnumber(L, bar->y);
    return (2);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_setx(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar = checkFoo(L, 1);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    bar->x = luaL_checkint(L, 2);
    lua_settop(L, 1);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_sety(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar = checkFoo(L, 1);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    bar->y = luaL_checkint(L, 2);
    lua_settop(L, 1);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_add(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar1 = checkFoo(L, 1);
    rumble_lua_userdata *bar2 = checkFoo(L, 2);
    rumble_lua_userdata *sum = pushFoo(L);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sum->x = bar1->x + bar2->x;
    sum->y = bar1->y + bar2->y;
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_dot(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_lua_userdata *bar1 = checkFoo(L, 1);
    rumble_lua_userdata *bar2 = checkFoo(L, 2);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    lua_pushnumber(L, bar1->x * bar2->x + bar1->y * bar2->y);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getdomains(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    cvector         *domains;
    rumble_domain   *domain;
    c_iterator      iter;
    int             x;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * rumble_lua_userdata *me = checkFoo(L, 1);
     */
    domains = rumble_domains_list();
    x = 0;
    cforeach((rumble_domain *), domain, domains, iter) {
        lua_pushstring(L, domain->name);
        x++;
    }

    return (x);
}

static const luaL_reg   Foo_methods[] =
{
    { "new", Foo_new },
    { "yourCfunction", Foo_yourCfunction },
    { "setx", Foo_setx },
    { "sety", Foo_sety },
    { "add", Foo_add },
    { "getdomains", rumble_lua_getdomains },
    { "SetHook", rumble_lua_hook_on_accept },
    { 0, 0 }
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_gc(lua_State *L) {
    printf("bye, bye, bar = %p\n", toFoo(L, 1));
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int Foo_tostring(lua_State *L) {

    /*~~~~~~~~~~~~~*/
    char    buff[32];
    /*~~~~~~~~~~~~~*/

    sprintf(buff, "%p", toFoo(L, 1));
    lua_pushfstring(L, "Foo (%s)", buff);
    return (1);
}

static const luaL_reg   Foo_meta[] = { { "__gc", Foo_gc }, { "__tostring", Foo_tostring }, { "__add", Foo_add }, { 0, 0 } };

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int Foo_register(lua_State *L) {
    luaL_register(L, FOO, Foo_methods); /* create methods table, add it to the globals */
    luaL_newmetatable(L, FOO);          /* create metatable for Foo, and add it to the Lua registry */
    luaL_register(L, 0, Foo_meta);      /* fill metatable */
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);   /* dup methods table */
    lua_rawset(L, -3);      /* metatable.__index = methods */
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);   /* dup methods table */
    lua_rawset(L, -3);      /* hide metatable: metatable.__metatable = methods */
    lua_pop(L, 1);          /* drop metatable */
    return (1); /* return methods on the stack */
}
#endif
