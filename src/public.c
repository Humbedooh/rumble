/* 
 * File:   public.c
 * Author: Humbedooh
 *
 * Created on January 7, 2011, 11:27 PM
 */

#include "rumble.h"
#include <inttypes.h>
/*
 * This file contains public functions for rumble (usable by both server and modules
 */

inline ssize_t rumble_comm_send(sessionHandle* session, const char* message) {
    return send(session->client->socket, message, strlen(message),0);
}

char* rumble_comm_read(sessionHandle* session) {
    char* ret = calloc(1,1025);
    char b = 0;
    uint32_t rc = 0;
    uint32_t p;
    for (p = 0; p < 1024; p++) {
        rc = recv(session->client->socket, &b, 1, 0);
        if ( !rc ) { free(ret); return NULL; }
        ret[p] = b;
        if ( b == '\n' ) break;
    }
    return ret;
}

void  rumble_string_lower(char* d) {
    int a,b;
    b = strlen(d);
    for (a = 0; a < b; a++) {
        d[a] = ( d[a] >= 'A'  && d[a] <= 'Z' ) ? d[a] + 32 : d[a];
    }
}

void  rumble_string_upper(char* d) {
    int a,b;
    b = strlen(d);
    for (a = 0; a < b; a++) {
        d[a] = ( d[a] >= 'a'  && d[a] <= 'z' ) ? d[a] - 32 : d[a];
    }
}


void rumble_scan_flags(cvector* dict, const char* flags){
    char* pch = strtok((char*) flags," ");
    while ( pch != NULL ) {
        if ( strlen(pch) >= 3 ) {
            configElement* entry = malloc(sizeof(configElement));
            entry->key = calloc(1, 100);
            entry->value = calloc(1, 100);
            sscanf(pch, "%100[^=]=%100c", (char*) entry->key, (char*) entry->value);
            rumble_string_upper((char*) entry->key);
            cvector_add(dict, entry);
        }
        pch = strtok(NULL, " ");
    }
}

const char* rumble_get_dictionary_value(cvector* dict, const char* flag){
    configElement* el;
    for ( el = (configElement*) cvector_first(dict); el != NULL; el = cvector_next(dict)) {
        if (!strcmp(flag, el->key)) return el->value;
    }
    return "";
}

void rumble_add_dictionary_value(cvector* dict, const char* key, const char* value) {
    char* nkey = calloc(1,strlen(key)+1);
    char* nval = calloc(1,strlen(value)+1);
    strcpy(nval, value);
    strcpy(nkey, key);
    configElement* el = malloc(sizeof(configElement));
    el->key = (const char*) nkey;
    el->value = (const char*) nval;
    cvector_add(dict, el);
}

void rumble_flush_dictionary(cvector* dict) {
    if (!dict) return;
    configElement* el;
    for ( el = (configElement*) cvector_first(dict); el != NULL; el = cvector_next(dict)) {
        if ( el->key  ) free((char*) el->key);
        if ( el->value) free((char*) el->value);
        free(el);
    }
    cvector_flush(dict);
}

void rumble_free_address(address* a) {
    if ( a->domain ) free(a->domain);
    if ( a->raw ) free(a->raw);
    if ( a->user ) free(a->user);
    if ( a->_flags ) free(a->_flags);
    rumble_flush_dictionary(a->flags);
    a->domain = 0;
    a->user = 0;
    a->raw = 0;
    a->_flags = 0;
}



char* rumble_mtime() {
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = gmtime ( &rawtime );
    char* moo = calloc(1,128);
    strftime(moo, 128, "%a, %d %b %Y %X +0000 (UTC)", timeinfo);
    //free(timeinfo);
    return moo;
}