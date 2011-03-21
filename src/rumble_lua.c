/*$I0 */
#include <sys/stat.h>
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
#ifdef RUMBLE_LUA
extern masterHandle *rumble_database_master_handle;
extern FILE             *sysLog;
#   define FOO "Rumble"
#   ifndef __STDC__
#      define __STDC__    1
#   endif
#   include <io.h>
#   include <fcntl.h>

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_panic(lua_State *L) {

    /*~~~~~~~~~~~~*/
    const char  *el;
    /*~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    el = lua_tostring(L, 1);
    printf("Lua PANIC: %s\n", el);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_fileinfo(lua_State *L)
{
#   if R_WINDOWS
#      define open    _open
#   endif

    /*~~~~~~~~~~~~~~~~~~*/
    struct stat fileinfo;
    const char  *filename;
    /*~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    filename = lua_tostring(L, 1);
    lua_settop(L, 0);
    if (stat(filename, &fileinfo) == -1) lua_pushnil(L);
    else {
        lua_newtable(L);
        lua_pushliteral(L, "size");
        lua_pushinteger(L, fileinfo.st_size);
        lua_rawset(L, -3);
        lua_pushliteral(L, "created");
        lua_pushinteger(L, fileinfo.st_ctime);
        lua_rawset(L, -3);
        lua_pushliteral(L, "modified");
        lua_pushinteger(L, fileinfo.st_mtime);
        lua_rawset(L, -3);
        lua_pushliteral(L, "accessed");
        lua_pushinteger(L, fileinfo.st_atime);
        lua_rawset(L, -3);
        lua_pushliteral(L, "mode");
        lua_pushinteger(L, fileinfo.st_mode);
        lua_rawset(L, -3);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_sethook(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    hookHandle      *hook = (hookHandle *) malloc(sizeof(hookHandle));
    char            svcName[32],
                    svcLocation[32],
                    svcCommand[32];
    rumbleService   *svc = 0;
    cvector         *svchooks = 0;
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

    svc = comm_serviceHandle(svcName);

    if (!strcmp(svcName, "smtp")) hook->flags |= RUMBLE_HOOK_SMTP;
    if (!strcmp(svcName, "pop3")) hook->flags |= RUMBLE_HOOK_POP3;
    if (!strcmp(svcName, "imap4")) hook->flags |= RUMBLE_HOOK_IMAP;

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

    cvector_add(svchooks, hook);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_send(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    const char      *message;
    sessionHandle   *session;
    int             n = 0;
    size_t          len = 0;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    n = lua_gettop(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
    session = (sessionHandle *) lua_topointer(L, -1);
    if (lua_type(L, 2) == LUA_TNUMBER) {
        len = luaL_optinteger(L, 2, 0);
        message = lua_tolstring(L, 3, &len);
        if (message) rumble_comm_send_bytes(session, message, len);
    } else {
        lua_settop(L, n);
        for (n = 2; n <= lua_gettop(L); n++) {

            /*
             * luaL_checktype(L, n, LUA_TSTRING);
             */
            message = lua_tostring(L, n);
            if (message) rcsend(session, message);
        }
    }

    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_deleteaccount(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char                      *user,
                                    *domain,
                                    *path;
    int                             uid = 0;
    void                            *state;
    rumble_mailbox                  *acc;
    rumble_mailman_shared_bag       *bag;
    rumble_mailman_shared_folder    *folder;
    rumble_letter                   *letter;
    char                            tmp[256];
    d_iterator                      diter,
                                    liter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (lua_type(L, 1) == LUA_TNUMBER) {
        uid = luaL_optinteger(L, 1, 0);
        state = rumble_database_prepare(0, "DELETE FROM accounts WHERE id = %u", uid);
        rumble_database_run(state);
        rumble_database_cleanup(state);
    } else {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        domain = lua_tostring(L, 1);
        user = lua_tostring(L, 2);
        acc = rumble_account_data(0, user, domain);
        if (acc) {
            path = strlen(acc->domain->path) ? acc->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
            bag = rumble_mailman_open_bag(acc->uid);
            dforeach(rmsf, folder, bag->folders, diter) {
                dforeach((rumble_letter *), letter, folder->letters, liter) {
                    sprintf(tmp, "%s/%s.msg", path, letter->fid);
                    unlink(tmp);
                }
            }

            state = rumble_database_prepare(0, "DELETE FROM accounts WHERE domain = %s AND user = %s", domain, user);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            state = rumble_database_prepare(0, "DELETE FROM mbox WHERE uid = %u", acc->uid);
            rumble_database_run(state);
            rumble_database_cleanup(state);
        }
    }

    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_recv(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    char            *line;
    sessionHandle   *session;
    size_t          len;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
    session = (sessionHandle *) lua_topointer(L, -1);
    line = rcread(session);
    if (line) {
        len = strlen(line);
        if (line[len - 1] == '\n') line[len - 1] = 0;
        if (line[len - 2] == '\r') line[len - 2] = 0;
        len = strlen(line);
    } else len = -1;
    lua_settop(L, 0);
    lua_pushstring(L, line);
    lua_pushinteger(L, len);
    return (2);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_recvbytes(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    char            *line;
    sessionHandle   *session;
    int             len;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TNUMBER);
    len = lua_tointeger(L, 2);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
    session = (sessionHandle *) lua_topointer(L, -1);
    line = rumble_comm_read_bytes(session, len);
    lua_settop(L, 0);
    lua_pushstring(L, line ? line : "");
    lua_pushinteger(L, line ? len : -1);
    return (2);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_sha256(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *string;
    char        *output;
    /*~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    string = lua_tostring(L, 1);
    output = rumble_sha256((const unsigned char *) string);
    lua_settop(L, 0);
    lua_pushstring(L, output);
    free(output);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_lock(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    sessionHandle   *session;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
    session = (sessionHandle *) lua_topointer(L, -1);
    svc = (rumbleService *) session->_svc;
    if (svc) pthread_mutex_lock(&svc->mutex);
    lua_pop(L, 1);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_unlock(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    sessionHandle   *session;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawgeti(L, 1, 0);
    luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
    session = (sessionHandle *) lua_topointer(L, -1);
    svc = (rumbleService *) session->_svc;
    if (svc) pthread_mutex_unlock(&svc->mutex);
    lua_pop(L, 1);
    return (0);
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
    lua_settop(L, 0);
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
        lua_pushliteral(L, "domain");
        lua_pushstring(L, domain);
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


static int rumble_lua_updatedomain(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    const char      *domain, *newname, *newpath, *newtype;
    void* state;
    rumble_domain* dmn;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    
    domain = lua_tostring(L, 1);
    newname = lua_tostring(L, 2);
    newpath = luaL_optstring(L, 3, "");
    newtype = luaL_optstring(L, 4, "");
    
    dmn = rumble_domain_copy(domain);
    if (dmn) {
        state = rumble_database_prepare(0, "UPDATE domains SET domain = %s, storagepath = %s WHERE id = %u", newname, newpath, dmn->id);
        rumble_database_run(state);
        rumble_database_cleanup(state);
        rumble_database_update_domains();
        state = rumble_database_prepare(0, "UPDATE accounts SET domain = %s WHERE domain = %s", newname, dmn->name);
        rumble_database_run(state);
        rumble_database_cleanup(state);
        free(dmn->name);
        if (dmn->path) free(dmn->path);
    }
    lua_settop(L, 0);    
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getaccount(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    const char      *domain,
                    *user;
    char            *mtype;
    rumble_mailbox  *acc;
    int             uid = 0;
    /*~~~~~~~~~~~~~~~~~~~~*/

    domain = lua_tostring(L, 1);
    user = lua_tostring(L, 2);
    uid = luaL_optinteger(L, 1, 0);
    acc = rumble_account_data(uid, domain, user);
    lua_settop(L, 0);
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
        lua_pushliteral(L, "domain");
        lua_pushstring(L, domain);
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

    lua_pushnil(L);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_saveaccount(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~*/
    const char  *user,
                *domain,
                *password,
                *arguments;
    const char  *mtype;
    int         x = 0;
    void        *state;
    uint32_t    uid = 0;
    /*~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TTABLE);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Get the account info
     -------------------------------------------------------------------------------------------------------------------
     */

    lua_pushliteral(L, "name");
    lua_gettable(L, -2);
    luaL_checktype(L, -1, LUA_TSTRING);
    user = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushliteral(L, "domain");
    lua_gettable(L, -2);
    luaL_checktype(L, -1, LUA_TSTRING);
    domain = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushliteral(L, "type");
    lua_gettable(L, -2);
    luaL_checktype(L, -1, LUA_TSTRING);
    mtype = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushliteral(L, "password");
    lua_gettable(L, -2);
    luaL_checktype(L, -1, LUA_TSTRING);
    password = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushliteral(L, "arguments");
    lua_gettable(L, -2);
    luaL_checktype(L, -1, LUA_TSTRING);
    arguments = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushliteral(L, "id");
    lua_gettable(L, -2);

    /*
     * luaL_checktype(L, -1, LUA_TNUMBER);
     */
    uid = luaL_optint(L, -1, 0);
    lua_settop(L, 0);
    if (rumble_domain_exists(domain)) {
        x = rumble_account_exists_raw(user, domain);
        if (uid && x) {
            state = rumble_database_prepare(0,
                                            "UPDATE accounts SET user = %s, domain = %s, type = %s, password = %s, arg = %s WHERE id = %u",
                                            user, domain, mtype, password, arguments, uid);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            lua_pushboolean(L, 1);
        } else if (!x) {
            state = rumble_database_prepare(0, "INSERT INTO ACCOUNTS (user,domain,type,password,arg) VALUES (%s,%s,%s,%s,%s)", user, domain,
                                            mtype, password, arguments);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            lua_pushboolean(L, 1);
        } else lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, 0);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_createdomain(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *domain,
                *path;
    void        *state;
    /*~~~~~~~~~~~~~~~~*/

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Check for duplicate name
     -------------------------------------------------------------------------------------------------------------------
     */

    luaL_checktype(L, 1, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    path = lua_tostring(L, 2);
    lua_settop(L, 0);
    if (!rumble_domain_exists(domain)) {
        state = rumble_database_prepare(0, "INSERT INTO domains (domain,storagepath) VALUES (%s,%s)", domain, path);
        rumble_database_run(state);
        rumble_database_cleanup(state);
        rumble_database_update_domains();
        lua_pushboolean(L, 1);
    } else lua_pushboolean(L, 0);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_deletedomain(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *domain;
    void        *state;
    /*~~~~~~~~~~~~~~~~*/

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Check for duplicate name
     -------------------------------------------------------------------------------------------------------------------
     */

    luaL_checktype(L, 1, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    lua_settop(L, 0);
    if (rumble_domain_exists(domain)) {
        state = rumble_database_prepare(0, "DELETE FROM domains WHERE domain = %s", domain);
        rumble_database_run(state);
        rumble_database_cleanup(state);
        rumble_database_update_domains();
        lua_pushboolean(L, 1);
    } else lua_pushboolean(L, 0);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_accountexists(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *user,
                *domain;
    /*~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    user = lua_tostring(L, 2);
    lua_settop(L, 0);
    if (rumble_account_exists_raw(user, domain)) lua_pushboolean(L, TRUE);
    else lua_pushboolean(L, FALSE);
    return (1);
}

static const luaL_reg   session_functions[] =
{
    { "lock", rumble_lua_lock },
    { "unlock", rumble_lua_unlock },
    { "send", rumble_lua_send },
    { "receive", rumble_lua_recv },
    { "receivebytes", rumble_lua_recvbytes },
    { 0, 0 }
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_addressexists(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *user,
                *domain;
    /*~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    user = lua_tostring(L, 2);
    lua_settop(L, 0);
    if (rumble_account_exists(0, user, domain)) lua_pushboolean(L, TRUE);
    else lua_pushboolean(L, FALSE);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *rumble_lua_handle_service(void *s) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc = (rumbleService *) s;
    masterHandle    *master = (masterHandle *) rumble_database_master_handle;
    /* Initialize a session handle and wait for incoming connections. */
    sessionHandle   session;
    sessionHandle   *sessptr = &session;
    d_iterator      iter;
    lua_State       *L;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    session.dict = dvector_init();
    session.recipients = dvector_init();
    session.client = (clientHandle *) malloc(sizeof(clientHandle));
    session.client->tls = 0;
    session.client->recv = 0;
    session.client->send = 0;
    session._master = master;
    session._tflags = 0;
    while (1) {
        comm_accept(svc->socket, session.client);
        pthread_mutex_lock(&svc->mutex);
        dvector_add(svc->handles, (void *) sessptr);
        pthread_mutex_unlock(&svc->mutex);
        session.flags = 0;
        session._tflags += 0x00100000;  /* job count ( 0 through 4095) */
        session.sender = 0;
        session._svc = s;
		//_CrtDumpMemoryLeaks
		
        /*$2
         ---------------------------------------------------------------------------------------------------------------
         Fetch an available Lua state
         ---------------------------------------------------------------------------------------------------------------
         */
        
        
        L = rumble_acquire_state();
        lua_settop(L, 0);
        lua_rawgeti(L, LUA_REGISTRYINDEX, svc->lua_handle);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Make a table for the session object and add the default session functions.
         ---------------------------------------------------------------------------------------------------------------
         */

        /*
         * lua_createtable(L, 32, 32);
         */
        lua_newtable(L);
        luaL_register(L, NULL, session_functions);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Push the session handle into the table as t[0].
         ---------------------------------------------------------------------------------------------------------------
         */

        lua_pushlightuserdata(L, &session);
        lua_rawseti(L, -2, 0);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Miscellaneous session data
         ---------------------------------------------------------------------------------------------------------------
         */

        lua_pushliteral(L, "protocol");
        lua_pushlstring(L, (session.client->client_info.ss_family == AF_INET6) ? "IPv6" : "IPv4", 4);
        lua_rawset(L, -3);
        lua_pushliteral(L, "address");
        lua_pushlstring(L, session.client->addr, strlen(session.client->addr));
        lua_rawset(L, -3);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Start the Lua function
         ---------------------------------------------------------------------------------------------------------------
         */

        lua_atpanic(L, rumble_lua_panic);
        if (lua_pcall(L, 1, 0, 0)) {
            rcprintf(&session, "\r\n\r\nLua error: %s!!\n", lua_tostring(L, -1));
        }

        /*
         * lua_close((L));
         * *pthread_mutex_unlock(&svc->mutex);
         */

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Clean up after the session
         ---------------------------------------------------------------------------------------------------------------
         */

        close(session.client->socket);
        rumble_clean_session(sessptr);
        lua_gc(L, LUA_GCCOLLECT, 0);
        rumble_release_state(L);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Update thread statistics
         ---------------------------------------------------------------------------------------------------------------
         */

        pthread_mutex_lock(&svc->mutex);
        foreach((sessionHandle *), s, svc->handles, iter) {
            if (s == sessptr) {
                dvector_delete(&iter);
                break;
            }
        }

        pthread_mutex_unlock(&svc->mutex);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_serverinfo(lua_State *L) {

    /*~~~~~~~~~~~~~*/
    char    tmp[256],
            *os;
    size_t  x,
            y;
    double  uptime;
    /*~~~~~~~~~~~~~*/

#   ifdef RUMBLE_MSC
#   endif
    sprintf(tmp, "%u.%02u.%04u", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    lua_newtable(L);
    lua_pushliteral(L, "version");
    lua_pushstring(L, tmp);
    lua_rawset(L, -3);
#   ifdef RUMBLE_MSC
    GetCurrentDirectoryA(256, tmp);
#   else
    getcwd(tmp, 256);
#   endif
    y = strlen(tmp);
    for (x = 0; x < y; x++)
        if (tmp[x] == '\\') tmp[x] = '/';
    lua_pushliteral(L, "path");
    lua_pushstring(L, tmp);
    lua_rawset(L, -3);
    uptime = difftime(time(0), rumble_database_master_handle->_core.uptime);
    lua_pushliteral(L, "uptime");
    lua_pushnumber(L, uptime);
    lua_rawset(L, -3);
    lua_pushliteral(L, "os");
    os = "POSIX compatible system";
    if (R_LINUX) os = "Linux";
    if (R_CYGWIN) os = "Cygwin";
    if (R_WINDOWS) os = "Windows";
    lua_pushstring(L, os);
    lua_rawset(L, -3);
    lua_pushliteral(L, "arch");
    lua_pushnumber(L, R_ARCH);
    lua_rawset(L, -3);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_serviceinfo(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    size_t          workers,
                    busy,
                    idle,
                    sessions,
                    out,
                    in;
    rumbleService   *svc = 0;
    char            capa[1024];
    const char      *svcName;
    c_iterator      iter;
    char            *c;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    svcName = lua_tostring(L, 1);
    svc = comm_serviceHandle(svcName);
    if (svc) {
        pthread_mutex_lock(&(svc->mutex));
        workers = svc->threads->size;   /* Number of threads alive */
        busy = svc->handles->size;      /* Number of threads busy */
        idle = workers - busy;          /* Number of threads idling */
        sessions = svc->traffic.sessions;
        out = svc->traffic.sent;
        in = svc->traffic.received;
        pthread_mutex_unlock(&(svc->mutex));
        lua_newtable(L);
        lua_pushliteral(L, "workers");
        lua_pushinteger(L, workers);
        lua_rawset(L, -3);
        lua_pushliteral(L, "busy");
        lua_pushinteger(L, busy);
        lua_rawset(L, -3);
        lua_pushliteral(L, "idle");
        lua_pushinteger(L, idle);
        lua_rawset(L, -3);
        lua_pushliteral(L, "enabled");
        lua_pushinteger(L, svc->enabled);
        lua_rawset(L, -3);
        lua_pushliteral(L, "sessions");
        lua_pushinteger(L, sessions);
        lua_rawset(L, -3);
        lua_pushliteral(L, "sent");
        lua_pushinteger(L, out);
        lua_rawset(L, -3);
        lua_pushliteral(L, "received");
        lua_pushinteger(L, in);
        lua_rawset(L, -3);
        memset(capa, 0, 1024);
        cforeach((char *), c, svc->capabilities, iter) {
            sprintf(&(capa[strlen(capa)]), "%s ", c);
        }

        lua_pushliteral(L, "capabilities");
        lua_pushstring(L, strlen(capa) ? capa : "");
        lua_rawset(L, -3);
        return (1);
    }

    lua_pushnil(L);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_listmodules(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_module_info  *mod;
    int                 x = 0;
    d_iterator          iter;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    lua_newtable(L);
    dforeach((rumble_module_info *), mod, rumble_database_master_handle->_core.modules, iter) {
        x++;
        lua_newtable(L);
        lua_pushliteral(L, "title");
        lua_pushstring(L, mod->title ? mod->title : "");
        lua_rawset(L, -3);
        lua_pushliteral(L, "description");
        lua_pushstring(L, mod->description ? mod->description : "");
        lua_rawset(L, -3);
        lua_pushliteral(L, "author");
        lua_pushstring(L, mod->author ? mod->author : "Unknown");
        lua_rawset(L, -3);
        lua_pushliteral(L, "file");
        lua_pushstring(L, mod->file ? mod->file : "");
        lua_rawset(L, -3);
        lua_rawseti(L, -2, x);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_gethostbyname(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    const char      *host;
    struct hostent  *server;
    /*~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    host = lua_tostring(L, 1);
    lua_settop(L, 0);
    server = gethostbyname(host);
    if (server) {
        lua_pushstring(L, inet_ntoa(*(struct in_addr *) *server->h_addr_list));
    } else lua_pushnil(L);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_config(lua_State *L) {

    /*~~~~~~~~~~~~*/
    const char  *el;
    /*~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    el = lua_tostring(L, 1);
    lua_settop(L, 0);
    if (rhdict(rumble_database_master_handle->_core.conf, el)) lua_pushstring(L, rrdict(rumble_database_master_handle->_core.conf, el));
    else lua_pushnil(L);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_fileexists(lua_State *L) {

    /*~~~~~~~~~~~~*/
    const char  *el;
    FILE        *fd;
    /*~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    el = lua_tostring(L, 1);
    lua_settop(L, 0);
#   ifdef RUMBLE_MSC
    if (access(el, 0) == 0) lua_pushboolean(L, 1);
#   else
    fd = fopen(el, "r");
    if (fd) {
        fclose(fd);
        lua_pushboolean(L, 1);
    }

    /*
     * if (faccessat(0, el, R_OK, AT_EACCESS) == 0) lua_pushboolean(L,1);
     */
#   endif
    else lua_pushboolean(L, 0);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_mx(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *host;
    d_iterator  iter;
    dvector     *mxlist;
    int         x = 0;
    mxRecord    *mx;
    /*~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    host = lua_tostring(L, 1);
    lua_settop(L, 0);
    lua_newtable(L);
    mxlist = comm_mxLookup(host);
    dforeach((mxRecord *), mx, mxlist, iter) {
        x++;
        lua_pushinteger(L, x);
        lua_newtable(L);
        lua_pushliteral(L, "preference");
        lua_pushinteger(L, mx->preference);
        lua_rawset(L, -3);
        lua_pushliteral(L, "host");
        lua_pushstring(L, mx->host);
        lua_rawset(L, -3);
        lua_rawset(L, -3);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_debug(lua_State *L) {

    /*~~~~~~~~~~~~*/
    const char  *el;
    /*~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    el = lua_tostring(L, 1);
    printf("Lua error: %s\n", el);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_createservice(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc;
    const char      *port;
    int             threads,
                    n;
    socketHandle    sock;
    int isFirstCaller = 0;
    /*~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TNUMBER);
    port = lua_tostring(L, 2);
    threads = luaL_optinteger(L, 3, 10);
    
    lua_rawgeti(L, LUA_REGISTRYINDEX,1);
    isFirstCaller = (lua_tointeger(L, -1) == 0) ? 1 : 0;

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Try to create a service at the given port before creating the service object
     -------------------------------------------------------------------------------------------------------------------
     */
    if (isFirstCaller) {
        sock = comm_init(rumble_database_master_handle, port);
        if (!sock) {
            lua_pushboolean(L, FALSE);
            return (1);
        }
    }
    /*$2
     -------------------------------------------------------------------------------------------------------------------
        If all went well, make the struct and set up stuff.
     -------------------------------------------------------------------------------------------------------------------
     */

    
    
    
    if (isFirstCaller) {
        lua_settop(L, 1);   /* Pop the stack so only the function ref is left. */
        svc = (rumbleService *) malloc(sizeof(rumbleService));
        svc->lua_handle = luaL_ref(L, LUA_REGISTRYINDEX);   /* Pop the ref and store it in the registry */
        svc->socket = sock;
        svc->cue_hooks = cvector_init();
        svc->init_hooks = cvector_init();
        svc->threads = dvector_init();
        svc->handles = dvector_init();
        svc->commands = cvector_init();
        svc->capabilities = cvector_init();
        svc->traffic.received = 0;
        svc->traffic.sent = 0;
        svc->traffic.sessions = 0;
        pthread_mutex_init(&svc->mutex, 0);
        for (n = 0; n < threads; n++) {
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            pthread_t   *t = (pthread_t *) malloc(sizeof(pthread_t));
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            dvector_add(svc->threads, t);
            pthread_create(t, NULL, rumble_lua_handle_service, svc);
        }
        lua_pushboolean(L, TRUE);
    }
    else {
        lua_settop(L, 1);   /* Pop the stack so only the function ref is left. */
        luaL_ref(L, LUA_REGISTRYINDEX);   /* Pop the ref and store it in the registry */
        lua_settop(L, 0);
        lua_pushnil(L);
    }
    
    return (1);
}

static int rumble_lua_reloadmodules(lua_State *L) {
    rumble_modules_load(rumble_database_master_handle);
    return 0;
}

static int rumble_lua_reloadconfig(lua_State *L) {
    rumble_config_load(rumble_database_master_handle,0);
    return 0;
}


static const luaL_reg   File_methods[] = { { "stat", rumble_lua_fileinfo }, { "exists", rumble_lua_fileexists }, { 0, 0 } };
static const luaL_reg   String_methods[] = { { "SHA256", rumble_lua_sha256 }, { 0, 0 } };
static const luaL_reg   Rumble_methods[] =
{
    { "createService", rumble_lua_createservice },
    { "readConfig", rumble_lua_config },
    { "setHook", rumble_lua_sethook },
    { "serverInfo", rumble_lua_serverinfo },
    { "serviceInfo", rumble_lua_serviceinfo },
    { "listModules", rumble_lua_listmodules },
    { "dprint", rumble_lua_debug },
    { "reloadModules", rumble_lua_reloadmodules},
    { "reloadConfiguration", rumble_lua_reloadconfig},
    { 0, 0 }
};
static const luaL_reg   Mailman_methods[] =
{
    { "listDomains", rumble_lua_getdomains },
    { "listAccounts", rumble_lua_getaccounts },
    { "readAccount", rumble_lua_getaccount },
    { "saveAccount", rumble_lua_saveaccount },
    { "deleteAccount", rumble_lua_deleteaccount },
    { "accountExists", rumble_lua_accountexists },
    { "addressExists", rumble_lua_addressexists },
    { "createDomain", rumble_lua_createdomain },
    { "deleteDomain", rumble_lua_deletedomain },
    { "updateDomain", rumble_lua_updatedomain},
    { 0, 0 }
};
static const luaL_reg   Network_methods[] = { { "getHostByName", rumble_lua_gethostbyname }, { "getMX", rumble_lua_mx }, { 0, 0 } };

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int Foo_register(lua_State *L) {
    lua_atpanic(L, rumble_lua_panic);
    luaL_register(L, "Mailman", Mailman_methods);   /* create methods table, add it to the globals */
    luaL_register(L, "Rumble", Rumble_methods);     /* create methods table, add it to the globals */
    luaL_register(L, "file", File_methods);         /* create methods table, add it to the globals */
    luaL_register(L, "string", String_methods);     /* create methods table, add it to the globals */
    luaL_register(L, "network", Network_methods);   /* create methods table, add it to the globals */
#   undef LUA_MINSTACK
#   define LUA_MINSTACK    50
    return (1); /* return methods on the stack */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
signed int rumble_lua_callback(lua_State *state, void *hook, void *session) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    lua_State       *L = lua_newthread(state);
    int             err_func;
    sessionHandle   *sess = (sessionHandle *) session;
    rumbleService   *svc = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    lua_atpanic(L, rumble_lua_panic);
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

    lua_pushlightuserdata(L, session);
    lua_rawseti(L, -2, 0);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Miscellaneous session data
     -------------------------------------------------------------------------------------------------------------------
     */

    lua_pushliteral(L, "protocol");
    lua_pushlstring(L, (sess->client->client_info.ss_family == AF_INET6) ? "IPv6" : "IPv4", 4);
    lua_rawset(L, -3);
    lua_pushliteral(L, "address");
    lua_pushlstring(L, sess->client->addr, strlen(sess->client->addr));
    lua_rawset(L, -3);
    lua_getglobal(L, "dprint");
    err_func = lua_gettop(L);

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Start the Lua function
     -------------------------------------------------------------------------------------------------------------------
     */

    lua_pcall(L, 1, 0, err_func);
    lua_settop(L, 0);

    /*$1
     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        Unlock the service mutex in case a Lua script forgot to
     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     */

    svc = (rumbleService *) sess->_svc;
    if (svc) pthread_mutex_unlock(&svc->mutex);

    /*
     * lua_gc(L, LUA_GCSTEP, 1);
     * ;
     * lua_close(L);
     */
    return (RUMBLE_RETURN_OKAY);
}
#endif
