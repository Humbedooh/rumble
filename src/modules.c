#include "rumble.h"
#if defined(_WIN32) && !defined(__CYGWIN__)

#else
#include <dlfcn.h>
#endif
#ifndef RTLD_NODELETE
#define RTLD_NODELETE 0x80
#endif

typedef int (*rumbleModInit)(void* master, rumble_module_info* modinfo);
typedef uint32_t (*rumbleVerCheck) (void);

void rumble_modules_load(masterHandle* master) {
    rumbleKeyValuePair* el;
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
    
	rumble_module_info* modinfo;
	char* error = 0;
    for ( line = master->_core.conf->first; line != NULL; line = line->next ) {
        el = (rumbleKeyValuePair*) line->object;
        if ( !strcmp(el->key, "loadmodule")) {
            printf("<modules> Loading %s...\n", el->value);
#if defined(_WIN32) && !defined(__CYGWIN__)
			handle = LoadLibraryA(el->value);

#else
            handle = dlopen(el->value, RTLD_LAZY | RTLD_NODELETE);
			error = dlerror();
#endif
            if (!handle) {
				error = error ? error : "(no such file?)";
                fprintf (stderr, "\n<modules> Error loading %s: %s\n", el->value, error);
                exit(1);
            }
            if ( error ) { printf("<modules> Warning: %s\n", error); }
            modinfo = (rumble_module_info*) calloc(1,sizeof(rumble_module_info));
			if (!modinfo) merror();
            modinfo->author = 0;
            modinfo->description = 0;
            modinfo->title = 0;
#if defined(_WIN32) && !defined(__CYGWIN__)
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
                master->_core.currentSO = el->value;
                cvector_add(master->_core.modules, modinfo);
                ver = (*mcheck)();
                if ( ver != RUMBLE_VERSION ) fprintf(stderr, "<modules> Warning: %s was compiled with librumble v%#x - current is %#x!\n<modules> Please recompile the module using the latest sources to avoid crashes or bugs.\n", el->value, ver, RUMBLE_VERSION);
                x = init(master, modinfo);
                if ( x != EXIT_SUCCESS ) { fprintf(stderr, "<modules> Error: %s failed to load!\n", el->value); 
#if defined(_WIN32) && !defined(__CYGWIN__)
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