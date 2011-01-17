/* 
 * File:   public.c
 * Author: Humbedooh
 *
 * Created on January 7, 2011, 11:27 PM
 */

#include "rumble.h"

/*
 * This file contains public functions for rumble (usable by both server and modules
 */

ssize_t rumble_comm_send(sessionHandle* session, const char* message) {
    return send(session->client->socket, message, strlen(message),0);
}

char* rumble_comm_read(sessionHandle* session) {
	char b = 0;
    uint32_t rc = 0;
    uint32_t p;
	char* ret = (char*) calloc(1,1025);
	if (!ret) { perror("Calloc failed!"); exit(1);}
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

/*
 * Scans a string for key=value pairs and stores them in a cvector dictionary.
 */
void rumble_scan_flags(cvector* dict, const char* flags){
    char* pch = strtok((char*) flags," ");
	char *key, *val;
	key = (char*) calloc(1,100);
	val = (char*) calloc(1,100);
    while ( pch != NULL ) {
        if ( strlen(pch) >= 3 ) {
			memset(key, 0, 99);
			memset(val, 0, 99);
            if (sscanf(pch, "%99[^=]=%99c", key, val) == 2) {
				rumble_string_upper(key);
				rsdict(dict, key, val);
			}
        }
        pch = strtok(NULL, " ");
    }
}

/*
 * Scans a string for words and stores them in a cvector dictionary as [word] => 1
 */
void rumble_scan_words(cvector* dict, const char* wordlist){
    char* pch = strtok((char*) wordlist," ");
    char *key= (char*) calloc(1,200);
    while ( pch != NULL ) {
		strncpy(key, pch, 200);
        if ( strlen(key) >= 1 ) { 
            rumble_string_lower(key);
            rsdict(dict, key, "1");
		}
        pch = strtok(NULL, " ");
    }
    
}

const char* rumble_get_dictionary_value(cvector* dict, const char* flag){
	cvector_element* curr;
    rumbleKeyValuePair* el;
	for ( curr = dict->first; curr != NULL; curr = curr->next) {
		el = (rumbleKeyValuePair*) curr->object;
        if (!strcmp(flag, el->key)) return el->value;
    }
    return "";
}

uint32_t rumble_has_dictionary_value(cvector* dict, const char* flag) {
	cvector_element* curr;
	for ( curr = dict->first; curr != NULL; curr = curr->next) {
        if (!strcmp(flag, (const char*) ((rumbleKeyValuePair*) curr->object)->key)) return 1;
    }
    return 0;
}

void rumble_add_dictionary_value(cvector* dict, const char* key, const char* value) {
    char* nkey, *nval;
	rumbleKeyValuePair* el;
	nkey = (char*) calloc(1,strlen(key)+1);
    nval = (char*) calloc(1,strlen(value)+1);
	if (!nkey || !nval) { perror("calloc() failed!"); exit(1); }
    strncpy(nval, value, strlen(value));
    strncpy(nkey, key, strlen(key));
    el = (rumbleKeyValuePair*) malloc(sizeof(rumbleKeyValuePair));
	if (!el) merror();
    el->key = (const char*) nkey;
    el->value = (const char*) nval;
    cvector_add(dict, el);
}

void rumble_flush_dictionary(cvector* dict) {
	rumbleKeyValuePair* el;
	if (!dict) { return; }
    for ( el = (rumbleKeyValuePair*) cvector_first(dict); el != NULL; el = (rumbleKeyValuePair*) cvector_next(dict)) {
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
	char* txt = (char*) calloc(1,128);
	if (!txt) { perror("calloc() failed!"); exit(1); }
    time ( &rawtime );
    timeinfo = gmtime ( &rawtime );
    strftime(txt, 128, "%a, %d %b %Y %X +0000 (UTC)", timeinfo);
    //free(timeinfo);
    return txt;
}