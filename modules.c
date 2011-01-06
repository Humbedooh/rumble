#include "rumble.h"
#include <dlfcn.h>

#ifndef RTLD_NODELETE
#define RTLD_NODELETE 0x80
#endif

void rumble_modules_load(masterHandle* master) {
    configElement* el;
    for ( el = cvector_first(master->readOnly.conf); el != NULL; el = cvector_next(master->readOnly.conf)) {
        if ( !strcmp(el->key, "loadmodule")) {
            printf("<modules> Loading %s...", el->value);
            void* handle = dlopen(el->value, RTLD_LAZY | RTLD_NODELETE);
            if (!handle) {
                fprintf (stderr, "\n<modules> Error loading %s: %s\n", el->value, dlerror());
                exit(1);
            }
            char* d = dlerror();    /* Clear any existing error */
            if ( d ) { printf("\n<modules> Warning: %s\n", d); }
            int (*init)(void* master);
            char* error;
            init = dlsym(handle, "rumble_module_init");
            if ((error = dlerror()) != NULL)  {
                fprintf (stderr, "\n<modules> Warning: %s does not contain any known calls.\n", el->value, error);
            }
            if ( init ) { 
                int x = (*init)(master);
                if ( x == EXIT_SUCCESS ) { printf("...OK.\n", el->value); }
                else { fprintf(stderr, "\n<modules> Error: %s failed to load!\n"); dlclose(handle); exit(EXIT_FAILURE); }
            }
            //dlclose(handle);
        }
    }
}