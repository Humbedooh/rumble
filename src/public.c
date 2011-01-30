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

address* rumble_parse_mail_address(const char* addr) {
	
	address* usr = (address*) malloc(sizeof(address));
	char* tmp;
	if (!usr) merror();
	usr->domain = (char*) calloc(1,128);
	usr->user = (char*) calloc(1,128);
	usr->raw = (char*) calloc(1,256);
	usr->flags = cvector_init();
	usr->tag = (char*) calloc(1,128);
	usr->_flags = (char*) calloc(1,128);
	tmp = (char*) calloc(1,256);
	
	if ( strchr(addr, '<') ) {
		addr = strchr(addr, '<');
		// First, try to scan for "stuff <user@domain> FLAGS"
		if ( sscanf(addr, "<%256[^@]@%128[^>]>%128[A-Z= %-]", tmp, usr->domain, usr->_flags) < 2) {
			// Then, try scanning for "<> FLAGS" (bounce message)
			sscanf(addr, "<%128[^>]>%128[A-Z= %-]", tmp, usr->_flags);
		}
		// Parse any flags we find
		if (strlen(usr->_flags)) rumble_scan_flags(usr->flags, usr->_flags);
		// Separate username and VERP/BATV tag (if any)
		if ( sscanf(tmp, "prvs=%16[^+=]=%128c", usr->tag, usr->user) < 2) { // Did we have a BATV tag?
			if ( sscanf(tmp, "%128[^=]=%128c", usr->tag, usr->user) < 2) { // Or maybe a VERP?
				strcpy(usr->user, tmp); // Nope, just a username.
				memset(usr->tag, 0, 128);
			}
		}
		sprintf(usr->raw, "<%s@%s> %s", usr->user, usr->domain, usr->_flags);
	}
	else if ( addr && strlen(addr) ) {
		sscanf(addr, "%256[^@]@%128c", usr->user, usr->domain);
	}
	free(tmp);
	return usr;
	
}

char* rumble_comm_read(sessionHandle* session) {
	char b = 0;
    ssize_t rc = 0;
    uint32_t p;
	struct timeval t;
	signed int f;
	char* ret = (char*) calloc(1,1025);
	
	if (!ret) { perror("Calloc failed!"); exit(1);}

	t.tv_sec = 5;
	t.tv_usec = 0;
	
    for (p = 0; p < 1024; p++) {
		f = select(session->client->socket+1, &session->client->fd, NULL, NULL, &t);
		if ( f > 0 ) {
			rc = recv(session->client->socket, &b, 1, 0);
			if ( rc <= 0 ) { free(ret); return NULL; }
			ret[p] = b;
			if ( b == '\n' ) break;
		}
		else {
			free(ret); return NULL;
		}
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
	if (!key) merror();
    while ( pch != NULL ) {
		strncpy(key, pch, 199);
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
    return "0";
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
	if (!nkey || !nval) merror();
    strcpy(nval, value);
    strcpy(nkey, key);

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
    if (!a) return;
    if ( a->domain ) free(a->domain);
    if ( a->raw ) free(a->raw);
    if ( a->user ) free(a->user);
    if ( a->_flags ) free(a->_flags);
	if ( a->tag ) free(a->tag);
    rumble_flush_dictionary(a->flags);
    a->domain = 0;
    a->user = 0;
    a->raw = 0;
    a->_flags = 0;
	a->tag = 0;
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