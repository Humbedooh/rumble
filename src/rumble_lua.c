/*$I0 */
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#include "rumble_version.h"
#include "mailman.h"
#include <sys/stat.h>
#ifdef RUMBLE_LUA
extern masterHandle *rumble_database_master_handle;
extern FILE         *sysLog;
extern dvector      *debugLog;
#   define FOO "Rumble"
#   ifndef __STDC__
#      define __STDC__    1
#   endif
#   if !defined(close)
#      include <fcntl.h>
#   endif
#   ifdef RUMBLE_MSC
#      include <windows.h>
#      include <tchar.h>
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static BOOL IsWow64(void) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    BOOL    bIsWow64 = FALSE;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * IsWow64Process is not available on all supported versions of Windows. ;
     * Use GetModuleHandle to get a handle to the DLL that contains the function ;
     * and GetProcAddress to get a pointer to the function if available
     */
    fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
    if (NULL != fnIsWow64Process) {
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64)) {

            /* handle error */
        }
    }

    return (bIsWow64);
}
#   endif

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
static int rumble_lua_debugLog(lua_State *L)
{
#   if R_WINDOWS
#      define open    _open
#   endif

    /*~~~~~~~~~~~~~~~*/
    d_iterator  diter;
    const char  *entry;
    int         x = 0;
    /*~~~~~~~~~~~~~~~*/

    lua_settop(L, 0);
    lua_newtable(L);
    dforeach((const char *), entry, debugLog, diter) {
        if (strlen(entry)) {
            x++;
            lua_pushinteger(L, x);
            lua_pushstring(L, entry);
            lua_rawset(L, -3);
        }
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
    int             isFirstCaller = 0;
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
    lua_rawgeti(L, LUA_REGISTRYINDEX, 1);
    isFirstCaller = (lua_tointeger(L, -1) == 0) ? 1 : 0;
    if (!isFirstCaller) {
        lua_settop(L, 0);
        lua_pushboolean(L, 0);
        return (1);
    }

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
        if (!strcmp(svcCommand, "helo")) hook->flags |= RUMBLE_CUE_SMTP_HELO;
        if (!strcmp(svcCommand, "ehlo")) hook->flags |= RUMBLE_CUE_SMTP_HELO;
        if (!strcmp(svcCommand, "mail")) hook->flags |= RUMBLE_CUE_SMTP_MAIL;
        if (!strcmp(svcCommand, "rcpt")) hook->flags |= RUMBLE_CUE_SMTP_RCPT;
        if (!strcmp(svcCommand, "data")) hook->flags |= RUMBLE_CUE_SMTP_DATA;
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

    hook->module = "Lua script";
    cvector_add(svchooks, hook);
    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return (1);
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

    /*
     * printf("+send\n");
     */
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
            if (message) rcsend(session, message ? message : "");
        }
    }

    lua_settop(L, 0);

    /*
     * printf("-send\n");
     */
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_deleteaccount(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    const char      *user,
                    *domain;
    int             uid = 0;
    rumble_mailbox  *acc;
    mailman_bag     *bag;
    mailman_folder  *folder;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (lua_type(L, 1) == LUA_TNUMBER) {
        uid = luaL_optinteger(L, 1, 0);
        acc = rumble_account_data(uid, 0, 0);
    } else {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        domain = lua_tostring(L, 1);
        user = lua_tostring(L, 2);
        acc = rumble_account_data(0, user, domain);
    }

    if (acc and acc->uid) {

        /*~~~~~~~~~~~~~~*/
        char    stmt[512];
        /*~~~~~~~~~~~~~~*/

        sprintf(stmt, "DELETE FROM accounts WHERE id = %u", acc->uid);
        radb_run(rumble_database_master_handle->_core.db, stmt);
        bag = mailman_get_bag(acc->uid, strlen(acc->domain->path) ? acc->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder"));
        rumble_debug(NULL, "Lua", "Deleted account: <%s@%s>", acc->user, acc->domain->name);
        if (bag) {

            /*~~~~~~~~~~*/
            /* TODO: Make it delete the folders and letters! */
            uint32_t    i;
            /*~~~~~~~~~~*/

            for (i = 0; i < bag->size; i++) {
                folder = &bag->folders[i];
                if (folder->inuse) {
                    mailman_update_folder(folder, bag->uid, 0); /* Make sure we get all letters first */
                    mailman_delete_folder(bag, folder);         /* Delete folder and its letters */
                }
            }
        }

        /* In case there's some leftover mails from old times? */
        radb_run_inject(rumble_database_master_handle->_core.mail, "DELETE FROM mbox WHERE uid = %u", acc->uid);
        mailman_close_bag(bag);
        rumble_free_account(acc);
    }

    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_recv(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    char            *line = 0;
    sessionHandle   *session;
    size_t          len;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

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
    } else len = 0;
    lua_settop(L, 0);
    lua_pushstring(L, line ? line : "");
    lua_pushinteger(L, len);
    if (line) free(line);
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
    if (line) free(line);
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
    output = rumble_sha256((const char *) string);
    lua_settop(L, 0);
    lua_pushstring(L, output);
    free(output);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_b64dec(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *string;
    char        *output;
    size_t      ilen,
                olen;
    /*~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    string = lua_tostring(L, 1);
    ilen = strlen(string);
    if (ilen) {
        output = malloc(ilen);
        olen = rumble_unbase64((unsigned char *) output, (const unsigned char *) string, ilen);
        lua_settop(L, 0);
        lua_pushlstring(L, output, olen);
        free(output);
    } else {
        lua_settop(L, 0);
        lua_pushliteral(L, "");
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_b64enc(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *string;
    char        *output;
    size_t      ilen;
    /*~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    string = lua_tostring(L, 1);
    ilen = strlen(string);
    if (ilen) {
        output = rumble_encode_base64(string, ilen);
        lua_settop(L, 0);
        lua_pushstring(L, output);
        free(output);
    } else {
        lua_settop(L, 0);
        lua_pushliteral(L, "");
    }

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
    lua_settop(L, 0);
    lua_newtable(L);
    cforeach((rumble_domain *), domain, domains, iter) {
        x++;

        /*
         * lua_pushinteger(L, x);
         */
        lua_pushstring(L, domain->name);
        lua_newtable(L);

        /*
         * lua_pushstring(L, "name");
         * lua_pushstring(L, domain->name);
         * lua_rawset(L, -3);
         */
        lua_pushstring(L, "path");
        lua_pushstring(L, domain->path);
        lua_rawset(L, -3);
        lua_pushstring(L, "flags");
        lua_pushinteger(L, domain->flags);
        lua_rawset(L, -3);
        lua_rawset(L, -3);

        /* Free up allocated memory */
        if (domain->path) free(domain->path);
        if (domain->name) free(domain->name);
        free(domain);
    }

    cvector_destroy(domains);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getmoduleconfig(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumblemodule_config_struct  *config;
    int                         x;
    const char                  *modName;
    rumble_module_info          *entry;
    rumble_module_info          *modInfo = 0;
    d_iterator                  iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    modName = lua_tostring(L, 1);
    if (!modName or!strlen(modName)) return (0);
    lua_settop(L, 0);
    dforeach((rumble_module_info *), entry, rumble_database_master_handle->_core.modules, iter) {
        if (entry->file && !strcmp(entry->file, modName)) {
            modInfo = entry;
            break;
        }
    }

    if (modInfo && modInfo->config) {
        x = 0;
        lua_newtable(L);
        config = modInfo->config(0, 0);
        if (config) {
            for (x = 0; config[x].key != 0; x++) {
                lua_pushinteger(L, x);
                lua_newtable(L);
                lua_pushstring(L, "key");
                lua_pushstring(L, config[x].key);
                lua_rawset(L, -3);
                lua_pushstring(L, "description");
                lua_pushstring(L, config[x].description);
                lua_rawset(L, -3);
                lua_pushstring(L, "length");
                lua_pushinteger(L, config[x].length);
                lua_rawset(L, -3);
                lua_pushstring(L, "type");
                if (config[x].type == RCS_STRING) lua_pushliteral(L, "string");
                if (config[x].type == RCS_BOOLEAN) lua_pushliteral(L, "boolean");
                if (config[x].type == RCS_NUMBER) lua_pushliteral(L, "number");
                lua_rawset(L, -3);
                lua_pushstring(L, "value");
                if (config[x].type == RCS_STRING) lua_pushstring(L, (const char *) config[x].value);
                if (config[x].type == RCS_BOOLEAN) lua_pushboolean(L, (int32_t) * ((int32_t *) config[x].value));
                if (config[x].type == RCS_NUMBER) lua_pushinteger(L, (int32_t) * ((int32_t *) config[x].value));
                lua_rawset(L, -3);
                lua_rawset(L, -3);
            }

            return (1);
        } else return (0);
    } else {
        return (0);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_setmoduleconfig(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char          *modName;
    const char          *key;
    const char          *value;
    rumble_module_info  *entry;
    rumble_module_info  *modInfo = 0;
    d_iterator          iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    modName = lua_tostring(L, 1);
    key = lua_tostring(L, 2);
    value = lua_tostring(L, 3);
    lua_settop(L, 0);
    if (!modName or!strlen(modName)) return (0);
    dforeach((rumble_module_info *), entry, rumble_database_master_handle->_core.modules, iter) {
        if (entry->file && !strcmp(entry->file, modName)) {
            modInfo = entry;
            break;
        }
    }

    if (modInfo && modInfo->config) modInfo->config(key, value);
    return (0);
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

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_updatedomain(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    const char      *domain,
                    *newname,
                    *newpath;
    uint32_t        flags;
    rumble_domain   *dmn;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    newname = lua_tostring(L, 2);
    newpath = luaL_optstring(L, 3, "");
    flags = luaL_optint(L, 4, 0);
    dmn = rumble_domain_copy(domain);
    if (dmn) {
        radb_run_inject(rumble_database_master_handle->_core.db,
                        "UPDATE domains SET domain = %s, storagepath = %s, flags = %u WHERE id = %u", newname, newpath, flags, dmn->id);
        rumble_database_update_domains();
        radb_run_inject(rumble_database_master_handle->_core.db, "UPDATE accounts SET domain = %s WHERE domain = %s", newname, dmn->name);
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

    if (lua_type(L, 1) == LUA_TNUMBER) {
        uid = luaL_optinteger(L, 1, 0);
    }

    domain = lua_tostring(L, 1);
    user = lua_tostring(L, 2);
    acc = rumble_account_data(uid, user, domain);
    lua_settop(L, 0);
    if (acc) {
        mtype = "unknown";
        switch (acc->type)
        {
        case RUMBLE_MTYPE_ALIAS:    mtype = "alias"; break;
        case RUMBLE_MTYPE_FEED:     mtype = "feed"; break;
        case RUMBLE_MTYPE_MBOX:     mtype = "mbox"; break;
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
        lua_pushstring(L, acc->domain->name);
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
        rumble_domain_free(acc->domain);
        return (1);
    }

    lua_pushnil(L);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getfolders(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~*/
    const char  *domain,
                *user;
    radbObject  *dbo = 0;
    radbResult  *dbr;
    int         uid = 0;
    /*~~~~~~~~~~~~~~~~~*/

    if (lua_type(L, 1) == LUA_TNUMBER) {
        uid = luaL_optinteger(L, 1, 0);
        dbo = radb_prepare(rumble_database_master_handle->_core.db, "SELECT id, name FROM folders WHERE uid = %u", uid);
    } else {
        domain = lua_tostring(L, 1);
        user = lua_tostring(L, 2);
        dbo = radb_prepare(rumble_database_master_handle->_core.db, "SELECT id, name FROM folders WHERE domain = %s AND user = %s", domain,
                           user);
    }

    lua_settop(L, 0);
    if (!dbo) return (0);
    lua_newtable(L);
    lua_pushinteger(L, 0);
    lua_pushliteral(L, "INBOX");
    lua_rawset(L, -3);
    while ((dbr = radb_fetch_row(dbo))) {
        lua_pushinteger(L, dbr->column[0].data.int64);
        lua_pushstring(L, dbr->column[1].data.string);
        lua_rawset(L, -3);
    }

    radb_cleanup(dbo);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_getheaders(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    radbObject  *dbo = 0;
    radbResult  *dbr;
    int         uid = 0;
    int64_t     folder = 0;
    char        filename[256],
                line[1024],
                key[1024],
                value[1024];
    const char  *path;
    FILE        *fp;
    /*
     * rumble_domain* domain = 0;
     */
    int         n = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    uid = luaL_optinteger(L, 1, 0);
    folder = luaL_optinteger(L, 2, 0);
    if (uid) {
        dbo = radb_prepare(rumble_database_master_handle->_core.mail,
                           "SELECT id, fid, size, delivered, flags FROM mbox WHERE uid = %u AND folder = %l", uid, folder);
    } else return (0);
    lua_settop(L, 0);
    if (!dbo) return (0);
    lua_newtable(L);

    /*
     * domain = rumble_domain_copy() ;
     * path = strlen(mbox->domain->path) ? mbox->domain->path :
     * rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
     */
    path = rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
    while ((dbr = radb_fetch_row(dbo))) {
        n++;
        lua_pushinteger(L, n);
        lua_newtable(L);
        lua_pushstring(L, "id");
        lua_pushinteger(L, dbr->column[0].data.uint64);
        lua_rawset(L, -3);
        lua_pushstring(L, "file");
        lua_pushstring(L, dbr->column[1].data.string);
        lua_rawset(L, -3);
        lua_pushstring(L, "size");
        lua_pushinteger(L, dbr->column[2].data.uint32);
        lua_rawset(L, -3);
        lua_pushstring(L, "sent");
        lua_pushinteger(L, dbr->column[3].data.uint32);
        lua_rawset(L, -3);
        lua_pushstring(L, "read");
        lua_pushboolean(L, (dbr->column[4].data.uint32 == RUMBLE_LETTER_READ) ? 1 : 0);
        lua_rawset(L, -3);
        sprintf(filename, "%s/%s.msg", path, dbr->column[1].data.string);
        fp = fopen(filename, "rb");
        if (fp) {
            while (!feof(fp)) {
                if (fgets(line, 1024, fp)) {
                    if (!strlen(line) || line[0] == '\r' || line[0] == '\n') break;
                    memset(key, 0, 1024);
                    memset(value, 0, 1024);
                    if (sscanf(line, "%128[^:]: %1000[^\r\n]", key, value) == 2) {
                        rumble_string_lower(key);
                        lua_pushstring(L, key);
                        lua_pushstring(L, value);
                        lua_rawset(L, -3);
                    }
                }
            }

            fclose(fp);
        }

        lua_rawset(L, -3);
    }

    radb_cleanup(dbo);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_deletemail(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    radbObject  *dbo = 0;
    radbResult  *dbr;
    int         uid = 0;
    int64_t     lid = 0;
    char        filename[256];
    const char  *path;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * rumble_domain* domain = 0;
     */
    uid = luaL_optinteger(L, 1, 0);
    lid = luaL_optinteger(L, 2, 0);
    if (uid && lid) {
        dbo = radb_prepare(rumble_database_master_handle->_core.mail, "SELECT fid FROM mbox WHERE id = %l AND uid = %uLIMIT 1", lid, uid);
    } else return (0);
    lua_settop(L, 0);
    if (!dbo) return (0);

    /*
     * domain = rumble_domain_copy() ;
     * path = strlen(mbox->domain->path) ? mbox->domain->path :
     * rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
     */
    path = rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
    dbr = radb_fetch_row(dbo);
    if (dbr) {
        sprintf(filename, "%s/%s.msg", path, dbr->column[0].data.string);

        /*
         * printf("Mailman.deleteMail: removing %s\n", filename);
         */
        unlink(filename);
        radb_run_inject(rumble_database_master_handle->_core.mail, "DELETE FROM mbox WHERE id = %l", lid);
    }

    radb_cleanup(dbo);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_lua_pushpart(lua_State *L, rumble_parsed_letter *letter) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    c_iterator              iter;
    rumbleKeyValuePair      *pair;
    rumble_parsed_letter    *child;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * Headers ;
     * printf("pushing headers\n");
     */
    lua_pushliteral(L, "headers");
    lua_newtable(L);
    cforeach((rumbleKeyValuePair *), pair, letter->headers, iter) {
        lua_pushstring(L, pair->key);
        lua_pushstring(L, pair->value);
        lua_rawset(L, -3);
    }

    lua_rawset(L, -3);

    /* Body */
    if (!letter->is_multipart) {

        /*
         * printf("pushing body\n");
         */
        lua_pushliteral(L, "body");
        lua_pushstring(L, letter->body);
        lua_rawset(L, -3);
    } /* Multipart */ else {

        /*~~~~~~*/
        /*
         * printf("pushing parts\n");
         */
        int k = 0;
        /*~~~~~~*/

        lua_pushliteral(L, "parts");
        lua_newtable(L);
        cforeach((rumble_parsed_letter *), child, letter->multipart_chunks, iter) {
            lua_pushinteger(L, ++k);
            lua_newtable(L);
            rumble_lua_pushpart(L, child);
            lua_rawset(L, -3);
        }

        /*
         * printf("Closing parts table..");
         */
        lua_rawset(L, -3);

        /*
         * printf("done\n");
         */
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_readmail(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    radbObject              *dbo = 0;
    radbResult              *dbr;
    int                     uid = 0;
    int64_t                 lid = 0;
    char                    filename[256];
    const char              *path;
    rumble_parsed_letter    *letter = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * rumble_domain* domain = 0;
     */
    uid = luaL_optinteger(L, 1, 0);
    lid = luaL_optinteger(L, 2, 0);
    if (uid && lid) {
        dbo = radb_prepare(rumble_database_master_handle->_core.mail,
                           "SELECT fid, size, delivered FROM mbox WHERE id = %l AND uid = %u LIMIT 1", lid, uid);
    } else return (0);
    lua_settop(L, 0);
    if (!dbo) return (0);

    /*
     * domain = rumble_domain_copy() ;
     * path = strlen(mbox->domain->path) ? mbox->domain->path :
     * rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
     */
    path = rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
    dbr = radb_fetch_row(dbo);
    if (dbr) {

        /*
         * printf("Found an email for parsing, getting info\n");
         */
        radb_run_inject(rumble_database_master_handle->_core.mail, "UPDATE mbox SET flags = %u WHERE id = %l", RUMBLE_LETTER_READ, lid);
        lua_newtable(L);
        lua_pushstring(L, "file");
        lua_pushstring(L, dbr->column[0].data.string);
        lua_rawset(L, -3);
        lua_pushstring(L, "size");
        lua_pushinteger(L, dbr->column[1].data.uint32);
        lua_rawset(L, -3);
        lua_pushstring(L, "sent");
        lua_pushinteger(L, dbr->column[2].data.uint32);
        lua_rawset(L, -3);

        /*
         * printf("Formatting filename\n");
         */
        sprintf(filename, "%s/%s.msg", path, dbr->column[0].data.string);

        /*
         * printf("Callung readmail()\n");
         */
        letter = rumble_mailman_readmail(filename);
        if (letter) {

            /*
             * printf("Creating letter struct for Lua\n");
             */
            rumble_lua_pushpart(L, letter);
            rumble_mailman_free_parsed_letter(letter);

            /*
             * printf("Done!\n");
             */
        }
    } else {
        lua_pushnil(L);
    }

    radb_cleanup(dbo);
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
        x = uid ? uid : rumble_account_exists_raw(user, domain);
        if (uid && x) {
            radb_run_inject(rumble_database_master_handle->_core.db,
                            "UPDATE accounts SET user = %s, domain = %s, type = %s, password = %s, arg = %s WHERE id = %u", user, domain,
                            mtype, password, arguments, uid);
            lua_pushboolean(L, 1);
        } else if (!x) {
            radb_run_inject(rumble_database_master_handle->_core.db,
                            "INSERT INTO ACCOUNTS (id,user,domain,type,password,arg) VALUES (NULL,%s,%s,%s,%s,%s)", user, domain, mtype,
                            password, arguments);
            lua_pushboolean(L, 1);
            rumble_debug(NULL, "Lua", "Created new account: <%s@%s>", user, domain);
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
    char xPath[512];
    uint32_t    flags;
    int bad = 0;
    /*~~~~~~~~~~~~~~~~*/

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Check for duplicate name
     -------------------------------------------------------------------------------------------------------------------
     */

    luaL_checktype(L, 1, LUA_TSTRING);
    domain = lua_tostring(L, 1);
    path = lua_tostring(L, 2);
    if (!path || !strlen(path)) {
        sprintf(xPath, "%s/%s", rrdict(rumble_database_master_handle->_core.conf, "storagefolder"), domain);
        rumble_debug(rumble_database_master_handle, "Lua", "Creating directory %s", xPath);
#ifdef RUMBLE_MSC
        bad = _mkdir(xPath);
#else
        bad = mkdir(xPath, S_IRWXU | S_IRGRP | S_IWGRP);
#endif
        path = xPath;
    }
    flags = luaL_optint(L, 3, 0);
    lua_settop(L, 0);
    if (!bad) {
        if (!rumble_domain_exists(domain)) {
            radb_run_inject(rumble_database_master_handle->_core.db, "INSERT INTO domains (id,domain,storagepath,flags) VALUES (NULL,%s,%s,%u)",
                            domain, path, flags);
            rumble_database_update_domains();
            lua_pushboolean(L, 1);
            rumble_debug(NULL, "Lua", "Created new domain: %s", domain);
        } else lua_pushboolean(L, 0);
    }
    else {
        printf("mkdir returned code %u!\n", bad);
        lua_pushboolean(L, 0);
    }
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_deletedomain(lua_State *L) {

    /*~~~~~~~~~~~~~~~~*/
    const char  *domain;
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
        radb_run_inject(rumble_database_master_handle->_core.db, "DELETE FROM domains WHERE domain = %s", domain);
        rumble_database_update_domains();
        lua_pushboolean(L, 1);
        rumble_debug(NULL, "Lua", "Deleted domain: %s", domain);
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

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_sendmail(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~*/
    const char  *message,
                *sf;
    char        *filename,
                *fid;
    FILE        *fp;
    address     *sender,
                *recipient;
    /*~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TSTRING);
    sender = rumble_parse_mail_address(lua_tostring(L, 1));
    recipient = rumble_parse_mail_address(lua_tostring(L, 2));
    if (sender and recipient) {
        message = lua_tostring(L, 3);
        fid = rumble_create_filename();
        sf = rumble_config_str(rumble_database_master_handle, "storagefolder");
        filename = (char *) calloc(1, strlen(sf) + 26);
        if (!filename) merror();
        sprintf(filename, "%s/%s", sf, fid);
        fp = fopen(filename, "wb");
        if (!fp) {
            lua_settop(L, 0);
            lua_pushboolean(L, FALSE);
        } else {
            fwrite(message, strlen(message), 1, fp);
            fclose(fp);
            radb_run_inject(rumble_database_master_handle->_core.mail, "INSERT INTO queue (fid, sender, recipient) VALUES (%s,%s,%s)", fid,
                            sender->raw, recipient->raw);
            lua_settop(L, 0);
            lua_pushstring(L, fid);
        }

        free(filename);
        free(fid);
    } else {
        lua_settop(L, 0);
        lua_pushboolean(L, FALSE);
    }

    /* Cleanup */
    rumble_free_address(sender);
    rumble_free_address(recipient);
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
    int             x = 0;
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

        /* CrtDumpMemoryLeaks */

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Fetch an available Lua state
         ---------------------------------------------------------------------------------------------------------------
         */

        L = rumble_acquire_state();

        /*
         * lua_settop(L, 0);
         */
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
        if ((x = lua_pcall(L, 1, 0, 0))) {
            rcprintf(&session, "\r\n\r\nLua error: %s!! (err %u)\n", lua_tostring(L, -1), x);
        }

        /*
         * lua_close((L));
         * pthread_mutex_unlock(&svc->mutex);
         */

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Clean up after the session
         ---------------------------------------------------------------------------------------------------------------
         */

        disconnect(session.client->socket);
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

    return (0);
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
    if (!getcwd(tmp, 256)) strcpy(tmp, "./");
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
#   ifdef RUMBLE_MSC
    if (R_ARCH == 32 && IsWow64()) lua_pushliteral(L, "WoW64");
    else lua_pushnumber(L, R_ARCH);
#   else
    lua_pushnumber(L, R_ARCH);
#   endif
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
                    in,
                    rej;
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
        rej = svc->traffic.rejections;
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
        lua_pushliteral(L, "rejected");
        lua_pushinteger(L, rej);
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
static int rumble_lua_trafficinfo(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    int             x = 0;
    rumbleService   *svc = 0;
    const char      *svcName;
    d_iterator      iter;
    traffic_entry   *tentry;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    svcName = lua_tostring(L, 1);
    svc = comm_serviceHandle(svcName);
    if (svc) {
        pthread_mutex_lock(&(svc->mutex));
        pthread_mutex_unlock(&(svc->mutex));
        lua_newtable(L);
        dforeach((traffic_entry *), tentry, svc->trafficlog, iter) {
            if (tentry && tentry->when) {
                x++;
                lua_pushinteger(L, x);
                lua_newtable(L);
                lua_pushinteger(L, 1);
                lua_pushinteger(L, tentry->hits);
                lua_rawset(L, -3);
                lua_pushinteger(L, 2);
                lua_pushinteger(L, tentry->bytes);
                lua_rawset(L, -3);
                lua_pushinteger(L, 3);
                lua_pushinteger(L, tentry->rejections);
                lua_rawset(L, -3);
                lua_rawset(L, -3);
            }
        }

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
    /*~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    el = lua_tostring(L, 1);
    lua_settop(L, 0);
#   ifdef RUMBLE_MSC
    if (access(el, 0) == 0) lua_pushboolean(L, 1);
#   else
    if (faccessat(0, el, R_OK, AT_EACCESS) == 0) lua_pushboolean(L, 1);
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
static int rumble_lua_suspendservice(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    const char      *svcName;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    svcName = lua_tostring(L, 1);
    svc = comm_serviceHandle(svcName);
    if (svc) comm_suspendService(svc);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_resumeservice(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    const char      *svcName;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    svcName = lua_tostring(L, 1);
    svc = comm_serviceHandle(svcName);
    if (svc) comm_resumeService(svc);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_killservice(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    const char      *svcName;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    svcName = lua_tostring(L, 1);
    svc = comm_serviceHandle(svcName);
    if (svc) comm_killService(svc);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_startservice(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    const char      *svcName;
    rumbleService   *svc;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TSTRING);
    svcName = lua_tostring(L, 1);
    svc = comm_serviceHandle(svcName);
    if (svc) comm_startService(svc);
    lua_settop(L, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_createservice(lua_State *L) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc;
    const char      *port;
    int             threads,
                    n;
    socketHandle    sock = 0;
    int             isFirstCaller = 0;
    pthread_attr_t  attr;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    luaL_checktype(L, 1, LUA_TFUNCTION);
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TNUMBER);
    port = lua_tostring(L, 2);
    threads = luaL_optinteger(L, 3, 10);
    lua_rawgeti(L, LUA_REGISTRYINDEX, 1);
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
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 128 * 1024);
        lua_settop(L, 1);   /* Pop the stack so only the function ref is left. */
        svc = (rumbleService *) malloc(sizeof(rumbleService));
        svc->lua_handle = luaL_ref(L, LUA_REGISTRYINDEX);   /* Pop the ref and store it in the registry */
        svc->socket = sock;
        svc->cue_hooks = cvector_init();
        svc->init_hooks = cvector_init();
        svc->threads = cvector_init();
        svc->handles = dvector_init();
        svc->commands = cvector_init();
        svc->capabilities = cvector_init();
        svc->traffic.received = 0;
        svc->traffic.sent = 0;
        svc->traffic.sessions = 0;
        pthread_mutex_init(&svc->mutex, 0);
        for (n = 0; n < threads; n++) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            rumbleThread    *thread = (rumbleThread *) malloc(sizeof(rumbleThread *));
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            cvector_add(svc->threads, thread);
            pthread_create(&thread->thread, &attr, rumble_lua_handle_service, svc);
        }

        lua_pushboolean(L, TRUE);
    } else {
        lua_settop(L, 1);   /* Pop the stack so only the function ref is left. */
        luaL_ref(L, LUA_REGISTRYINDEX); /* Pop the ref and store it in the registry */
        lua_settop(L, 0);
        lua_pushnil(L);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_reloadmodules(lua_State *L) {
    rumble_modules_load(rumble_database_master_handle);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_lua_reloadconfig(lua_State *L) {
    rumble_config_load(rumble_database_master_handle, 0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
signed int rumble_lua_callback(lua_State *state, void *hook, void *session) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    lua_State       *L = lua_newthread(state);
    sessionHandle   *sess = (sessionHandle *) session;
    rumbleService   *svc = 0;
    int             rc = RUMBLE_RETURN_OKAY;
    int             type = 0;
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

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Start the Lua function
     -------------------------------------------------------------------------------------------------------------------
     */

    if (lua_pcall(L, 1, 1, 0)) {
        fprintf(stderr, "Lua error: %s!!\n", lua_tostring(L, -1));
    }

    type = lua_type(L, -1);
    if (type == LUA_TBOOLEAN) rc = lua_toboolean(L, -1) ? RUMBLE_RETURN_OKAY : RUMBLE_RETURN_FAILURE;
    if (type == LUA_TNUMBER) rc = luaL_optint(L, -1, RUMBLE_RETURN_OKAY);
    if (type == LUA_TSTRING) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        const char  *str = luaL_optstring(L, -1, "okay");
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if (str) {
            if (!strcmp(str, "okay")) rc = RUMBLE_RETURN_OKAY;
            if (!strcmp(str, "failure")) rc = RUMBLE_RETURN_FAILURE;
            if (!strcmp(str, "ignore")) rc = RUMBLE_RETURN_IGNORE;
        }
    }

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
    return (rc);
}
#endif
static const luaL_reg   File_methods[] = { { "stat", rumble_lua_fileinfo }, { "exists", rumble_lua_fileexists }, { 0, 0 } };
static const luaL_reg   String_methods[] =
{
    { "SHA256", rumble_lua_sha256 },
    { "decode64", rumble_lua_b64dec },
    { "encode64", rumble_lua_b64enc },
    { 0, 0 }
};
static const luaL_reg   Rumble_methods[] =
{
    { "createService", rumble_lua_createservice },
    { "suspendService", rumble_lua_suspendservice },
    { "resumeService", rumble_lua_resumeservice },
    { "stopService", rumble_lua_killservice },
    { "startService", rumble_lua_startservice },
    { "readConfig", rumble_lua_config },
    { "setHook", rumble_lua_sethook },
    { "serverInfo", rumble_lua_serverinfo },
    { "serviceInfo", rumble_lua_serviceinfo },
    { "trafficInfo", rumble_lua_trafficinfo },
    { "listModules", rumble_lua_listmodules },
    { "dprint", rumble_lua_debug },
    { "reloadModules", rumble_lua_reloadmodules },
    { "reloadConfiguration", rumble_lua_reloadconfig },
    { "getLog", rumble_lua_debugLog },
    { "getmoduleconfig", rumble_lua_getmoduleconfig },
    { "setmoduleconfig", rumble_lua_setmoduleconfig },
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
    { "updateDomain", rumble_lua_updatedomain },
    { "listFolders", rumble_lua_getfolders },
    { "listHeaders", rumble_lua_getheaders },
    { "sendMail", rumble_lua_sendmail },
    { "readMail", rumble_lua_readmail },
    { "deleteMail", rumble_lua_deletemail },
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
#undef LUA_MINSTACK
#define LUA_MINSTACK    50
    return (1); /* return methods on the stack */
}
