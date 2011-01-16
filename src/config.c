#include "rumble.h"
#include <stdarg.h>

void rumble_config_load(masterHandle* master, cvector* args) {
    char* paths[] = { "config", "/var/rumble/config" };
    char* cfgfile = (char*) calloc(1,1024);
	FILE* config;
	configElement* el;
	master->readOnly.conf = cvector_init();
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
        for ( x = 0; x < 2; x++ ) {
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
        char* buffer = (char*) malloc(512);
        int p = 0;
        while (!feof(config)) {
            memset(buffer, 0, 512);
            fgets(buffer, 512,config);
            p++;
            if ( !ferror(config) ) {
                char* variable = (char*) calloc(1, 512);
                char* value = (char*) calloc(1, 512);
                sscanf(buffer, "%511[^# \t]%*[ \t]%511[^\r\n]", variable, value );
                if ( strlen(variable) > 2 ) {
                    rumble_string_lower(variable);
                    el = (configElement*) malloc(sizeof(configElement));
                    el->key = variable;
                    el->value = value;
                    cvector_add(master->readOnly.conf, el);
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
    return (const char*) "0";
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