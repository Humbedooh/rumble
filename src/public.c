/* 
 * File:   public.c
 * Author: Humbedooh
 *
 * Created on January 7, 2011, 11:27 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "rumble.h"
#include <inttypes.h>
#include <openssl/sha.h>
#include <unistd.h>
#include "sqlite3.h"
/*
 * This file contains public functions for rumble (usable by both server and modules
 */

inline ssize_t rumble_comm_send(sessionHandle* session, const char* message) {
    return send(session->client->socket, message, strlen(message),0);
}

char* rumble_comm_read(sessionHandle* session) {
    char* ret = calloc(1,1025);
    char b = 0;
    ssize_t rc = 0;
    uint32_t p;
    for (p = 0; p < 1024; p++) {
        rc = read(session->client->socket, &b, 1);
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

/* char* rumble_sha256(const unsigned char* d)
 * Converts the string (d) into a SHA-256 digest (64 byte hex string).
 * Note: For extra speed, digests are printed out "backwards" as:
 * DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA
 * This should have no effect on security and is roughly 3 times faster.
*/
char* rumble_sha256(const unsigned char* d) {
    unsigned char* md = malloc(33);
    char* ret = malloc(65);
    SHA256(d, strlen((const char*) d), md);
    unsigned int* x = (unsigned int*) md;
    sprintf((char*) ret, "%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32, x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7]);
    memset(md, 0, 33); // Erase md, just in case.
    free(md);
    return ret;
}

/* char* rumble_sha160(const unsigned char* d)
 * Converts the string (d) into a hex SHA1 160 bit digest (40 byte hex string).
 * This is used for simpler tasks, such as grey-listing, where collisions are
 * of less importance.
 * Note: For extra speed, digests are printed out "backwards" as:
 * DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA
 * This should have no effect on security and is roughly 3 times faster.
*/
char* rumble_sha160(const unsigned char* d) {
    unsigned char* md = malloc(21);
    char* ret = malloc(41);
    SHA1(d, strlen((const char*) d), md);
    unsigned int* x = (unsigned int*) md;
    sprintf((char*) ret, "%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32, x[0],x[1],x[2],x[3],x[4]);
    memset(md, 0, 21); // Erase md, just in case.
    free(md);
    return ret;
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
    char* nkey = malloc(strlen(key));
    char* nval = malloc(strlen(value));
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
        free((char*) el->key);
        free((char*) el->value);
        free(el);
        cvector_delete(dict);
    }
}

void rumble_free_address(address* a) {
    if ( a->domain ) free(a->domain);
    if ( a->raw ) free(a->raw);
    if ( a->user ) free(a->user);
    rumble_flush_dictionary(a->flags);
    a->domain = 0;
    a->user = 0;
    a->raw = 0;
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