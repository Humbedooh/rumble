#include "rumble.h"
#ifdef _WIN32

#else
#include <dlfcn.h>
#endif
#ifndef RTLD_NODELETE
#define RTLD_NODELETE 0x80
#endif

typedef int (*rumbleModInit)(void* master, rumble_module_info* modinfo);
typedef uint32_t (*rumbleVerCheck) (void);

void rumble_modules_load(masterHandle* master) {
    configElement* el;
    cvector_element* line;
	uint32_t ver;
	int x;
#ifdef _WIN32
	HINSTANCE handle;
#else
	void* handle;
#endif
	rumbleModInit init;
	rumbleVerCheck mcheck;
    char* error;
	rumble_module_info* modinfo;
    for ( line = master->readOnly.conf->first; line != NULL; line = line->next ) {
        el = (configElement*) line->object;
        if ( !strcmp(el->key, "loadmodule")) {
            printf("<modules> Loading %s...\n", el->value);
			#ifdef _WIN32
			handle = LoadLibraryA(el->value);
			error = "(no such file?)";
#else
            handle = dlopen(el->value, RTLD_LAZY | RTLD_NODELETE);
			error = dlerror();
#endif
            if (!handle) {
                fprintf (stderr, "\n<modules> Error loading %s: %s\n", el->value, error);
                exit(1);
            }
            if ( error ) { printf("<modules> Warning: %s\n", error); }
            modinfo = (rumble_module_info*) calloc(1,sizeof(rumble_module_info));
            modinfo->author = 0;
            modinfo->description = 0;
            modinfo->title = 0;
#ifdef _WIN32
			init = (rumbleModInit) GetProcAddress(handle, "rumble_module_init");
			mcheck = (rumbleVerCheck) GetProcAddress(handle, "rumble_module_check");
			error = ( init == 0 || mcheck == 0 ) ? "no errors" : 0;
#else
            init = dlsym(handle, "rumble_module_init");
            mcheck = dlsym(handle, "rumble_module_check");
			error = dlerror();
#endif
            if (error != NULL)  {
                fprintf (stderr, "<modules> Warning: %s does not contain required module functions.\n", el->value);
            }
            if ( init && mcheck ) { 
                master->readOnly.currentSO = el->value;
                cvector_add(master->readOnly.modules, modinfo);
                ver = (*mcheck)();
                if ( ver != RUMBLE_VERSION ) fprintf(stderr, "<modules> Warning: %s was compiled with librumble v%#x - current is %#x!\n<modules> Please recompile the module using the latest sources to avoid crashes or bugs.\n", el->value, ver, RUMBLE_VERSION);
                x = init(master, modinfo);
                if ( x != EXIT_SUCCESS ) { fprintf(stderr, "<modules> Error: %s failed to load!\n", el->value); 
#ifdef _WIN32
				FreeLibrary(handle);
#else
				dlclose(handle);
#endif
				exit(EXIT_FAILURE); }
            }
            modinfo->file = el->value;
            //dlclose(handle);
        }
    }
}