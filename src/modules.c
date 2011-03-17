/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "rumble_version.h"
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

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_modules_load(masterHandle *master, FILE* runlog) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
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
    char                *error = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
                exit(1);
            }

            if (error) {
                printf("Warning: %s\n", error);
            }

            modinfo = (rumble_module_info *) calloc(1, sizeof(rumble_module_info));
            if (!modinfo) merror();
            modinfo->author = 0;
            modinfo->description = 0;
            modinfo->title = 0;
            init = (rumbleModInit) dlsym(handle, "rumble_module_init");
            mcheck = (rumbleVerCheck) dlsym(handle, "rumble_module_check");
            error = (init == 0 || mcheck == 0) ? "no errors" : 0;
            if (error != NULL) fprintf(stderr, "\nWarning: %s does not contain required module functions.\n", el->value);
            if (init && mcheck) {
                master->_core.currentSO = el->value;
                dvector_add(master->_core.modules, modinfo);
                ver = (*mcheck) ();
                ver = (ver & 0xFFFFFF00) + (RUMBLE_VERSION & 0x000000FF);
                x = EXIT_SUCCESS;
                if (ver != RUMBLE_VERSION) {
                    if (ver > RUMBLE_VERSION) {
                        fprintf(stderr,
                                "\nError: %s was compiled with a newer version of librumble (v%#X) than this server executable (v%#X).\nPlease recompile the module using the latest sources to avoid crashes or bugs.\n",
                            el->value, ver, RUMBLE_VERSION);
                    } else {
                        fprintf(stderr,
                                "\nError: %s was compiled with an older version of librumble (v%#X).\nPlease recompile the module using the latest sources (v%#X) to avoid crashes or bugs.\n",
                            el->value, ver, RUMBLE_VERSION);
                    }
                } else x = init(master, modinfo);
                if (x != EXIT_SUCCESS) {
                    fprintf(stderr, "\nError: %s failed to load!\n", el->value);
                    dlclose(handle);
                }

                if (x == EXIT_SUCCESS) {
                    if (modinfo->title) printf("Loaded extension: %-30s[OK]\n", modinfo->title);
                    else printf("%48s[OK]\n", el->value);
                } else printf("[BAD]\n");
            }

            modinfo->file = el->value;

            /*
             * dlclose(handle);
             */
        }

#ifdef RUMBLE_LUA
        else if (!strcmp(el->key, "loadscript")) {

            /*~~~~~~~~~~~*/
            lua_State   *L;
            /*~~~~~~~~~~~*/

            printf("Loading script <%s>\n", el->value);
            L = (lua_State *) master->_core.lua;
            if (!L) {
                master->_core.lua = (lua_State *) luaL_newstate();
                L = (lua_State *) master->_core.lua;
                luaL_openlibs(L);
                luaopen_debug(L);
                Foo_register(L);
            }

            if (luaL_loadfile(L, el->value)) {
                fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
            } else if (lua_pcall(L, 0, LUA_MULTRET, 0)) {
                fprintf(stderr, "Failed to run <%s>: %s\n", el->value, lua_tostring(L, -1));
            }
        }
#endif
    }
}
