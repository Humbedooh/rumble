#include "rumble.h"
#include <stdarg.h>

void rumble_config_load(masterHandle* master, cvector* args) {
    char* paths[3] = { "config", "/var/rumble/config", "C:/cygwin/home/Administrator/rumble/config" };
    char* cfgfile;
	FILE* config;
	rumbleKeyValuePair* el;
	master->_core.conf = cvector_init();
	cfgfile = (char*) calloc(1,1024);
	if (!cfgfile) merror();
    if ( strlen(rumble_get_dictionary_value(args, "--CONFIG-DIR"))) {
        el = (rumbleKeyValuePair*) malloc(sizeof(rumbleKeyValuePair));
		if (!el) merror();
        el->key = "config-dir";
        el->value = rumble_get_dictionary_value(args, "--CONFIG-DIR");
        cvector_add(master->_core.conf, el);
        sprintf(cfgfile, "%s/rumble.conf", el->value);
        master->cfgdir = el->value;
    }
    else {
        int x = 0;
        for ( x = 0; x < 3; x++ ) {
            sprintf(cfgfile, "%s/rumble.conf", paths[x]);
            config = fopen(cfgfile, "r");     
            if ( config ) {
                fclose(config);
                el = (rumbleKeyValuePair*) malloc(sizeof(rumbleKeyValuePair));
                el->key = "config-dir";
                el->value = paths[x];
                cvector_add(master->_core.conf, el);
                master->cfgdir = el->value;
                break;
            }
        }
    }
    config = fopen(cfgfile, "r");
    if ( config ) {
		int p = 0;
		char *key, *value, *buffer;
        buffer = (char*) malloc(512);
		key = (char*) calloc(1,512);
		value = (char*) calloc(1,512);
		if(!buffer||!key||!value) merror();
        while (!feof(config)) {
            memset(buffer, 0, 512);
            fgets(buffer, 511,config);
            p++;
            if ( !ferror(config) ) {
                memset(key, 0, 512);
				memset(value, 0, 512);
                if ( sscanf(buffer, "%511[^# \t]%*[ \t]%511[^\r\n]", key, value ) == 2) {
                    rumble_string_lower(key);
                    rsdict(master->_core.conf, key, value);
                }
            }
            else {
                fprintf(stderr, "<config> Error: Could not read %s!\n", cfgfile);
                exit(EXIT_FAILURE);
            }
        }
        free(buffer);
        fclose(config);
    }
    else {
        fprintf(stderr, "<config> Error: Could not read %s!\n", cfgfile);
        exit(EXIT_FAILURE);
    }
}

const char* rumble_config_str(masterHandle* master, const char* key) {
    rumbleKeyValuePair* el;
    for ( el = (rumbleKeyValuePair*) cvector_first(master->_core.conf); el != NULL; el = (rumbleKeyValuePair*) cvector_next(master->_core.conf)) {
        if ( !strcmp(el->key, key)) {
            return (const char*) el->value;
        }
    }
    return (const char*) "";
}

uint32_t rumble_config_int(masterHandle* master, const char* key) {
    rumbleKeyValuePair* el;
    for ( el = (rumbleKeyValuePair*) cvector_first(master->_core.conf); el != NULL; el = (rumbleKeyValuePair*) cvector_next(master->_core.conf)) {
        if ( !strcmp(el->key, key)) {
            return atoi(el->value);
        }
    }
    return 0;
}