#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cvector.h"
#include "rumble.h"

extern masterHandle* master_ext_copy;

void  rumble_strlower(char* d) {
    int a,b;
    b = strlen(d);
    for (a = 0; a < b; a++) {
        d[a] = ( d[a] >= 'A'  && d[a] <= 'Z' ) ? d[a] + 32 : d[a];
    }
}

void  rumble_strupper(char* d) {
    int a,b;
    b = strlen(d);
    for (a = 0; a < b; a++) {
        d[a] = ( d[a] >= 'a'  && d[a] <= 'z' ) ? d[a] - 32 : d[a];
    }
}

void rumble_config_load(masterHandle* master) {
    master->readOnly.conf = cvector_init();
    FILE* config = fopen("config/rumble.conf", "r");
    if ( config ) {
        char* buffer = malloc(512);
        int p = 0;
        char byte;
        while (!feof(config)) {
            memset(buffer, 0, 512);
            fgets(buffer, 512,config);
            if ( !ferror(config) ) {
                char* variable = calloc(1, 512);
                char* value = calloc(1, 512);
                sscanf(buffer, "%511[^# \t]%*[ \t]%511[^\r\n]", variable, value );
                if ( strlen(variable) ) {
                    rumble_strlower(variable);
                    configElement* el = malloc(sizeof(configElement));
                    el->key = variable;
                    el->value = value;
                    cvector_add(master->readOnly.conf, el);
                }
            }
            else {
                perror("<config> Error: Could not read rumble.conf");
                exit(EXIT_FAILURE);
            }
        }
        free(buffer);
        fclose(config);
    }
    else {
        perror("<config> Error: Could not read configuration file <rumble.conf>");
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