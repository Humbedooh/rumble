/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "rumble_version.h"
#include "comm.h"
#if defined(_WIN32) && !defined(__CYGWIN__)
#   define dlclose FreeLibrary
#   define dlsym   GetProcAddress
#   include <Windows.h>
#else
#   include <dlfcn.h>
#endif
#ifndef RTLD_NODELETE
#   define RTLD_NODELETE   0x80
#endif
typedef int (*rumbleModInit) (void *master, rumble_module_info *modinfo);
typedef uint32_t (*rumbleVerCheck) (void);
typedef rumblemodule_config_struct * (*rumbleModConfig) (const char *key, const char *value);
extern FILE *sysLog;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_modules_load(masterHandle *master) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleKeyValuePair  *el;
    dvector_element     *line;
    uint32_t            ver;
    int                 x;
#ifdef _WIN32
    HINSTANCE           handle;
#else
    void                *handle;
#endif
    rumbleModInit       init;
    rumbleVerCheck      mcheck;
    rumble_module_info  *modinfo;
    rumbleService       *svc;
    char                *error = 0;
    const char          *services[] = { "mailman", "smtp", "pop3", "imap4", 0 };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_debug(NULL, "core", "Preparing to load modules");
    for (x = 0; services[x]; x++) {
        svc = comm_serviceHandle(services[x]);
        if (svc) {
            rumble_debug(NULL, "core", "Flushing hook structs for %s", services[x]);
            svc->cue_hooks = cvector_init();
            svc->init_hooks = cvector_init();
            svc->exit_hooks = cvector_init();
        }
    }

    rumble_debug(NULL, "core", "Loading modules");
    for (line = master->_core.conf->first; line != NULL; line = line->next) {
        el = (rumbleKeyValuePair *) line->object;
        if (!strcmp(el->key, "loadmodule"))
        {
#if R_WINDOWS
            handle = LoadLibraryA(el->value);
#else
            handle = dlopen(el->value, RTLD_LAZY | RTLD_NODELETE);
            error = dlerror();
#endif
            if (!handle) {
                error = error ? error : "(no such file?)";
                fprintf(stderr, "\nError loading %s: %s\n", el->value, error);
                rumble_debug(NULL, "core", "Error loading %s: %s", el->value, error);
                exit(1);
            }

            if (error) {
                rumble_debug(NULL, "core", "Warning: %s\n", error);
            }

            modinfo = (rumble_module_info *) calloc(1, sizeof(rumble_module_info));
            if (!modinfo) merror();
            modinfo->author = 0;
            modinfo->description = 0;
            modinfo->title = 0;
            init = (rumbleModInit) dlsym(handle, "rumble_module_init");
            mcheck = (rumbleVerCheck) dlsym(handle, "rumble_module_check");
            modinfo->config = (rumbleModConfig) dlsym(handle, "rumble_module_config");
            error = (init == 0 || mcheck == 0) ? "no errors" : 0;
            if (error != NULL) {
                rumble_debug(NULL, "core", "Warning: %s does not contain required module functions.\n", el->value);
            }

            if (init && mcheck) {
                master->_core.currentSO = el->value;
                dvector_add(master->_core.modules, modinfo);
                ver = (*mcheck) ();
                ver = (ver & 0xFFFFFF00) + (RUMBLE_VERSION & 0x000000FF);
                x = EXIT_SUCCESS;
                if (ver > RUMBLE_VERSION || ver < RUMBLE_VERSION_REQUIRED) {
                    if (ver > RUMBLE_VERSION) {
                        rumble_debug(NULL, "module",
                                     "Error: %s was compiled with a newer version of librumble (v%#X) than this server executable (v%#X).\nPlease recompile the module using the latest sources to avoid crashes or bugs.\n",
                                 el->value, ver, RUMBLE_VERSION);
                    } else {
                        rumble_debug(NULL, "module",
                                     "Error: %s was compiled with an older version of librumble (v%#X).\nPlease recompile the module using the latest sources (v%#X) to avoid crashes or bugs.\n",
                                 el->value, ver, RUMBLE_VERSION);
                    }
                } else {
                    modinfo->file = el->value;
                    x = init(master, modinfo);
                }

                if (x != EXIT_SUCCESS) {
                    rumble_debug(NULL, "module", "Error: %s failed to load!", el->value);
                    dlclose(handle);
                }

                if (x == EXIT_SUCCESS) {
                    if (modinfo->title) rumble_debug(NULL, "module", "Loaded extension: %-30s", modinfo->title);
                    else rumble_debug(NULL, "module", "Loaded %48s", el->value);
                } else rumble_debug(NULL, "module", "%s exited prematurely!", el->value);
            }

            /*
             * dlclose(handle);
             */
        }

#ifdef RUMBLE_LUA
        else if (!strcmp(el->key, "loadscript")) {

            /*~~~~~~~~~~~*/
            int         x;
            lua_State   *L;
            /*~~~~~~~~~~~*/

            for (x = 0; x < RUMBLE_LSTATES; x++) {
                if (!master->lua.states[x].state) {
                    master->lua.states[x].state = luaL_newstate();
                    L = (lua_State *) master->lua.states[x].state;
                    lua_pushinteger(L, x);
                    luaL_ref(L, LUA_REGISTRYINDEX);
                    luaL_openlibs(L);
                    luaopen_debug(L);
                    Foo_register(L);
                }
            }

            printf("Loading script <%s>\n", el->value);

            /* Load the file into all states */
            for (x = 0; x < RUMBLE_LSTATES; x++) {
                L = (lua_State *) master->lua.states[x].state;
                if (luaL_loadfile(L, el->value)) {
                    rumble_debug(NULL, "lua", "Couldn't load script: %s", lua_tostring(L, -1));
                    break;
                } else if (lua_pcall(L, 0, LUA_MULTRET, 0)) {
                    rumble_debug(NULL, "lua", "Failed to run <%s>: %s\n", el->value, lua_tostring(L, -1));
                    break;
                }
            }
        }
#endif
    }
}
