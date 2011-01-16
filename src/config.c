#include "rumble.h"
#include <stdarg.h>

void rumble_config_load(masterHandle* master, cvector* args) {
    char* paths[3] = { "config", "/var/rumble/config", "C:/cygwin/home/Administrator/rumble/config" };
    char* cfgfile;
	FILE* config;
	configElement* el;
	master->readOnly.conf = cvector_init();
	cfgfile = (char*) calloc(1,1024);
    if ( strlen(rumble_get_dictionary_value(args, "--CONFIG-DIR"))) {
        el = (configElement*) malloc(sizeof(configElement));
        el->key = "config-dir";
        el->value = rumble_get_dictionary_value(args, "--CONFIG-DIR");
        cvector_add(master->readOnly.conf, el);
        sprintf(cfgfile, "%s/rumble.conf", el->value);
        master->cfgdir = el->value;
    }
    else {
        int x = 0;
	configElement* el;
        for ( x = 0; x < 3; x++ ) {
            sprintf(cfgfile, "%s/rumble.conf", paths[x]);
            config = fopen(cfgfile, "r");     
            if ( config ) {
                fclose(config);
                el = (configElement*) malloc(sizeof(configElement));
                el->key = "config-dir";
                el->value = paths[x];
                cvector_add(master->readOnly.conf, el);
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
        while (!feof(config)) {
            memset(buffer, 0, 512);
            fgets(buffer, 511,config);
            p++;
            if ( !ferror(config) ) {
                memset(key, 0, 512);
				memset(value, 0, 512);
                sscanf(buffer, "%511[^# \t]%*[ \t]%511[^\r\n]", key, value );
                if ( strlen(key) > 2 && strlen(value) > 0 ) {
                    rumble_string_lower(key);
                    rsdict(master->readOnly.conf, key, value);
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
    configElement* el;
    for ( el = (configElement*) cvector_first(master->readOnly.conf); el != NULL; el = (configElement*) cvector_next(master->readOnly.conf)) {
        if ( !strcmp(el->key, key)) {
            return (const char*) el->value;
        }
    }
    return (const char*) "";
}

uint32_t rumble_config_int(masterHandle* master, const char* key) {
    configElement* el;
    for ( el = (configElement*) cvector_first(master->readOnly.conf); el != NULL; el = (configElement*) cvector_next(master->readOnly.conf)) {
        if ( !strcmp(el->key, key)) {
            return atoi(el->value);
        }
    }
    return 0;
}