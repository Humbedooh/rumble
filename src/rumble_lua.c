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
     * lua_setmetatable(L, -2);
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
    char            svcName[32],
                    svcLocation[32],
                    svcCommand[32];
    rumbleService   *svc = 0;
    cvector         *svchooks = 0;
    int             len;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    hook->flags = 0;
    hook->func = 0;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TSTRING);
    memset(svcName, 0, 32);
    memset(svcLocation, 0, 32);
    memset(svcCommand, 0, 32);
    strncpy(svcName, lua_tostring(L, 2), 31);
    strncpy(svcLocation, lua_tostring(L, 3), 31);
    strncpy(svcCommand, luaL_optstring(L, 4, "smurf"), 31);
    rumble_string_lower(svcName);
    rumble_string_lower(svcLocation);
    rumble_string_lower(svcCommand);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Check which service to hook onto
     -------------------------------------------------------------------------------------------------------------------
     */

    if (!strcmp(svcName, "smtp")) {
        svc = &rumble_database_master_handle->smtp;
        hook->flags |= RUMBLE_HOOK_SMTP;
    }

    if (!strcmp(svcName, "pop3")) {
        svc = &rumble_database_master_handle->pop3;
        hook->flags |= RUMBLE_HOOK_POP3;
    }

    if (!strcmp(svcName, "imap4")) {
        svc = &rumble_database_master_handle->imap;
        hook->flags |= RUMBLE_HOOK_IMAP;
    }

    if (!svc) {
        luaL_error(L, "\"%s\" isn't a known service - choices are: smtp, imap4, pop3.", svcName);
        return (0);
    }

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Check which location in the service to hook onto
     -------------------------------------------------------------------------------------------------------------------
     */

    if (!strcmp(svcLocation, "accept")) {
        svchooks = svc->init_hooks;
        hook->flags |= RUMBLE_HOOK_ACCEPT;
    }

    if (!strcmp(svcLocation, "close")) {
        svchooks = svc->exit_hooks;
        hook->flags |= RUMBLE_HOOK_CLOSE;
    }

    if (!strcmp(svcLocation, "command")) {
        svchooks = svc->cue_hooks;
        hook->flags |= RUMBLE_HOOK_COMMAND;
    }

    if (!svchooks) {
        luaL_error(L, "\"%s\" isn't a known hooking location - choices are: accept, close, command.", svcLocation);
        return (0);
    }

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        If hooking to a command, set it
     -------------------------------------------------------------------------------------------------------------------
     */

    if (svchooks == svc->cue_hooks) { }

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Save the callback reference in the Lua registry for later use
     -------------------------------------------------------------------------------------------------------------------
     */

    lua_settop(L, 1);   /* Pop the stack so only the function ref is left. */
    hook->lua_callback = luaL_ref(L, LUA_REGISTRYINDEX);    /* Pop the ref and store it in the registry */

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Save the hook in the appropriate cvector and finish up
     -------------------------------------------------------------------------------------------------------------------
     */

    printf("Adding hook as callback %d\n", hook->lua_callback);
    cvector_add(svchooks, hook);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_send(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char          *message;
    rumble_lua_session  *session;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("sending msg from lua\n");
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TUSERDATA);
    session = rumble_lua_session_get(L, -1);
    message = lua_tostring(L, 2);
    rcsend(session->session, message);
    lua_pop(L, 2);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_recv(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char                *line;
    rumble_lua_session  *session;
    int                 len;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TUSERDATA);
    session = rumble_lua_session_get(L, -1);
    line = rcread(session->session);
    if (line) {
        len = strlen(line);
        if (line[len - 1] == '\n') line[len - 1] = 0;
        if (line[len - 2] == '\r') line[len - 2] = 0;
        len = strlen(line);
    } else len = -1;
    lua_pop(L, 1);
    lua_pushstring(L, line);
    lua_pushinteger(L, len);
    return (2);
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

    domains = rumble_domains_list();
    x = 0;
    lua_newtable(L);
    cforeach((rumble_domain *), domain, domains, iter) {
        x++;
        lua_pushinteger(L, x);
        lua_pushstring(L, domain->name);
        lua_rawset(L, -3);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getaccounts(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    cvector         *accounts;
    const char      *domain;
    char            *mtype;
    c_iterator      iter;
    rumble_mailbox  *acc;
    int             x = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    lua_pop(L, 1);
    accounts = rumble_database_accounts_list(domain);
    lua_newtable(L);
    cforeach((rumble_mailbox *), acc, accounts, iter) {
        mtype = "unknown";
        switch (acc->type)
        {
        case RUMBLE_MTYPE_ALIAS:    mtype = "alias"; break;
        case RUMBLE_MTYPE_FEED:     mtype = "feed"; break;
        case RUMBLE_MTYPE_MBOX:     mtype = "mailbox"; break;
        case RUMBLE_MTYPE_MOD:      mtype = "module"; break;
        case RUMBLE_MTYPE_RELAY:    mtype = "relay"; break;
        default:                    break;
        }

        x++;
        lua_pushinteger(L, x);
        lua_newtable(L);
        lua_pushliteral(L, "id");
        lua_pushinteger(L, acc->uid);
        lua_rawset(L, -3);
        lua_pushliteral(L, "name");
        lua_pushstring(L, acc->user);
        lua_rawset(L, -3);
        lua_pushliteral(L, "password");
        lua_pushstring(L, acc->hash);
        lua_rawset(L, -3);
        lua_pushliteral(L, "type");
        lua_pushstring(L, mtype);
        lua_rawset(L, -3);
        lua_pushliteral(L, "arguments");
        lua_pushstring(L, acc->arg);
        lua_rawset(L, -3);
        lua_rawset(L, -3);
    }

    rumble_database_accounts_free(accounts);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getaccount(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    const char      *user, *domain;
    char            *mtype;
    rumble_mailbox  *acc;
    int             x = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    user = lua_tostring(L, 2);
    lua_pop(L, 2);
    acc = rumble_account_data(0, user, domain);
    if (acc) {
        mtype = "unknown";
        switch (acc->type)
        {
        case RUMBLE_MTYPE_ALIAS:    mtype = "alias"; break;
        case RUMBLE_MTYPE_FEED:     mtype = "feed"; break;
        case RUMBLE_MTYPE_MBOX:     mtype = "mailbox"; break;
        case RUMBLE_MTYPE_MOD:      mtype = "module"; break;
        case RUMBLE_MTYPE_RELAY:    mtype = "relay"; break;
        default:                    break;
        }
        
        lua_newtable(L);
        lua_pushliteral(L, "id");
        lua_pushinteger(L, acc->uid);
        lua_rawset(L, -3);
        lua_pushliteral(L, "name");
        lua_pushstring(L, acc->user);
        lua_rawset(L, -3);
        lua_pushliteral(L, "password");
        lua_pushstring(L, acc->hash);
        lua_rawset(L, -3);
        lua_pushliteral(L, "type");
        lua_pushstring(L, mtype);
        lua_rawset(L, -3);
        lua_pushliteral(L, "arguments");
        lua_pushstring(L, acc->arg);
        lua_rawset(L, -3);
        rumble_free_account(acc);
        return (1);
    }
    return 0;
}

static const luaL_reg   Foo_methods[] =
{
    { "listDomains", rumble_lua_getdomains },
    { "listAccounts", rumble_lua_getaccounts },
    { "readAccount", rumble_lua_getaccount },
    { "SetHook", rumble_lua_hook_on_accept },
    { 0, 0 }
};
static const luaL_reg   Foo_meta[] = { { 0, 0 } };  /* { "__tostring", Foo_tostring }, { 0, 0 }
                                                     * };
                                                     * */

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
static const luaL_reg   session_functions[] = { { "Send", rumble_lua_send }, { "Receive", rumble_lua_recv }, { 0, 0 } };

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_lua_callback(lua_State *L, void *hook, sessionHandle *session) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ((hookHandle *) hook)->lua_callback);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Make a table for the session object and add the default session functions.
     -------------------------------------------------------------------------------------------------------------------
     */

    lua_createtable(L, 0, 0);
    luaL_register(L, 0, session_functions);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Push the session handle into the table as t[0].
     -------------------------------------------------------------------------------------------------------------------
     */

    rumble_lua_session_create(L, session);
    lua_rawseti(L, -2, 0);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Start the Lua function
     -------------------------------------------------------------------------------------------------------------------
     */

    lua_pcall(L, 1, 0, 0);
    return (RUMBLE_RETURN_OKAY);
}
