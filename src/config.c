#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cvector.h"
#include "rumble.h"
extern masterHandle* master_ext_copy;
extern cvector* args;

void rumble_config_load(masterHandle* master) {
    master->readOnly.conf = cvector_init();
    char* paths[] = { "config", "/var/rumble/config" };
    char* cfgfile = calloc(1,1024);
    if ( strlen(rumble_get_dictionary_value(args, "--CONFIG-DIR"))) {
        configElement* el = malloc(sizeof(configElement));
        el->key = "config-dir";
        el->value = rumble_get_dictionary_value(args, "--CONFIG-DIR");
        cvector_add(master->readOnly.conf, el);
        sprintf(cfgfile, "%s/rumble.conf", el->value);
        master->cfgdir = el->value;
    }
    else {
        int x = 0;
        for ( x = 0; x < 2; x++ ) {
            sprintf(cfgfile, "%s/rumble.conf", paths[x]);
            FILE* config = fopen(cfgfile, "r");     
            if ( config ) {
                fclose(config);
                configElement* el = malloc(sizeof(configElement));
                el->key = "config-dir";
                el->value = paths[x];
                cvector_add(master->readOnly.conf, el);
                master->cfgdir = el->value;
                break;
            }
        }
    }
    FILE* config = fopen(cfgfile, "r");
    if ( config ) {
        char* buffer = malloc(512);
        int p = 0;
        while (!feof(config)) {
            memset(buffer, 0, 512);
            fgets(buffer, 512,config);
            p++;
            if ( !ferror(config) ) {
                char* variable = calloc(1, 512);
                char* value = calloc(1, 512);
                sscanf(buffer, "%511[^# \t]%*[ \t]%511[^\r\n]", variable, value );
                if ( strlen(variable) ) {
                    rumble_string_lower(variable);
                    configElement* el = malloc(sizeof(configElement));
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

const char* rumble_config_str(const char* key) {
    masterHandle* master = master_ext_copy;
    configElement* el;
    for ( el = cvector_first(master->readOnly.conf); el != NULL; el = cvector_next(master->readOnly.conf)) {
        if ( !strcmp(el->key, key)) {
            return (const char*) el->value;
        }
    }
    return (const char*) "0";
}

uint32_t rumble_config_int(const char* key) {
    masterHandle* master = master_ext_copy;
    configElement* el;
    for ( el = cvector_first(master->readOnly.conf); el != NULL; el = cvector_next(master->readOnly.conf)) {
        if ( !strcmp(el->key, key)) {
            return atoi(el->value);
        }
    }
    return 0;
}