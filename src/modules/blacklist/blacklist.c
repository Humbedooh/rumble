/* 
 * File:   blacklist.c
 * Author: Humbedooh
 * 
 * A simple black-listing module for rumble.
 *
 * Created on January 3, 2011, 8:08 PM
 */

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "../../rumble.h"
cvector* blacklist_baddomains;
cvector* blacklist_badhosts;
cvector* blacklist_dnsbl;
unsigned int blacklist_spf = 0;
const char* blacklist_logfile = 0;

ssize_t rumble_blacklist_domains(sessionHandle* session) {
    cvector_element* el;
    // Check against pre-configured list of bad hosts
    if ( cvector_size(blacklist_baddomains)) {
        el = blacklist_baddomains->first;
        while ( el ) {
            char* badhost = (char*) el->object;
            if ( !strcmp(session->sender.domain, badhost) ) {
                #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
                printf("<blacklist> %s was blacklisted as a bad domain, aborting\n", badhost);
                #endif
                rumble_comm_send(session,"530 Sender domain has been blacklisted.\r\n");
                if ( blacklist_logfile ) { 
                    FILE* fp = fopen(blacklist_logfile, "w+");
                    if (fp) {
                        time_t rawtime;
                        time(&rawtime);
                        fprintf(fp, "<blacklist>[%s] %s: Attempt to use %s as sender domain.\r\n", ctime(&rawtime), session->client->addr, badhost);
                        fclose(fp);
                    }
                }
                return RUMBLE_RETURN_IGNORE;
            }
            el = el->next;
        }
    }
    return RUMBLE_RETURN_OKAY;
}

ssize_t rumble_blacklist(sessionHandle* session) {
    // First, check if the client has been given permission to skip this check by any other modules.
    if ( session->flags & RUMBLE_SMTP_FREEPASS ) return RUMBLE_RETURN_OKAY;
    
    // Resolve client address name
    struct hostent* client;
    struct in6_addr IP;
    inet_pton(session->client->client_info.ss_family, session->client->addr, &IP);
    client = gethostbyaddr((char *)&IP, (session->client->client_info.ss_family == AF_INET) ? 4 : 16, session->client->client_info.ss_family);
    const char* addr = (const char*) client->h_name;
    rumble_string_lower((char*) addr);
    
    cvector_element* el;
    // Check against pre-configured list of bad hosts
    if ( cvector_size(blacklist_badhosts)) {
        el = blacklist_badhosts->first;
        while ( el ) {
            char* badhost = (char*) el->object;
            if ( strstr(addr, badhost) ) {
                #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
                printf("<blacklist> %s was blacklisted as a bad host name, aborting\n", addr);
                #endif
                if ( blacklist_logfile ) { 
                    FILE* fp = fopen(blacklist_logfile, "w+");
                    if (fp) {
                        time_t rawtime;
                        time(&rawtime);
                        fprintf(fp, "<blacklist>[%s] %s: %s is blacklisted as a bad host.\r\n",  ctime(&rawtime), session->client->addr, addr);
                        fclose(fp);
                    }
                }
                return RUMBLE_RETURN_FAILURE;
            }
            el = el->next;
        }
        
    }
    
    // Check against DNS blacklists
    if ( cvector_size(blacklist_dnsbl) ) {
        if ( session->client->client_info.ss_family == AF_INET ) { // I only know how to match IPv4 DNSBL :/
            unsigned int a,b,c,d;
            sscanf(session->client->addr, "%3u.%3u.%3u.%3u",&a,&b,&c,&d);
            el = blacklist_dnsbl->first;
            char* dnshost = (char*) el->object;
            while ( dnshost ) {
                char* dnsbl = calloc(1, strlen(dnshost) + strlen(addr) + 6);
                sprintf(dnsbl, "%d.%d.%d.%d.%s", d,c,b,a, dnshost);
                struct hostent* bl = gethostbyname(dnsbl);
                if ( bl) { 
                    #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
                    printf("<blacklist> %s was blacklisted by %s, closing connection!\n", addr, dnshost);
                    #endif
                    if ( blacklist_logfile ) { 
                        FILE* fp = fopen(blacklist_logfile, "w+");
                        if (fp) {
                            time_t rawtime;
                            time(&rawtime);
                            fprintf(fp, "<blacklist>[%s] %s: %s is blacklisted by DNSBL %s.\r\n",  ctime(&rawtime), session->client->addr, addr, dnshost);
                            fclose(fp);
                        }
                    }
                    free(dnsbl);
                    return RUMBLE_RETURN_FAILURE;
                } // Blacklisted, abort the connection!
                free(dnsbl);
                el = el->next;
                if ( el ) dnshost = el->object;
                else dnshost = NULL;
            }
        }
    }
    
    free(client);
    // Return with EXIT_SUCCESS and let the server continue.
    return RUMBLE_RETURN_OKAY;
}

int rumble_module_init(void* master, rumble_module_info* modinfo) {
    modinfo->title = "Blacklisting module";
    modinfo->description = "Standard blacklisting module for rumble.";
    blacklist_badhosts = cvector_init();
    blacklist_baddomains = cvector_init();
    blacklist_dnsbl = cvector_init();
    char* cfgfile = calloc(1,1024);
    sprintf(cfgfile, "%s/blacklist.conf", ((masterHandle*) master)->cfgdir);
    FILE* config = fopen(cfgfile, "r");
    if ( config ) {
        char* buffer = malloc(4096);
        int p = 0;
        char byte;
        while (!feof(config)) {
            memset(buffer, 0, 4096);
            fgets(buffer, 4096,config);
            if ( !ferror(config) ) {
                char* key = calloc(1,100);
                char* val = calloc(1,3900);
                sscanf(buffer, "%100[^# \t\r\n] %3900[^\r\n]", key, val );
                rumble_string_lower(key);
                if ( !strcmp(key, "dnsbl") ) {
                    char* pch = strtok((char*) val," ");
                    while ( pch != NULL ) {
                        if ( strlen(pch) >= 4) {
                            char* entry = calloc(1, strlen(pch)+1);
                            strncpy(entry, pch, strlen(pch));
                            cvector_add(blacklist_dnsbl, entry);
                            pch = strtok(NULL, " ");
                        }
                    }
                }
                if ( !strcmp(key, "enablespf")) blacklist_spf = atoi(val);
                if ( !strcmp(key, "blacklistbyhost")) {
                    char* pch = strtok((char*) val," ");
                    while ( pch != NULL ) {
                        if ( strlen(pch) >= 4) {
                            char* entry = calloc(1, strlen(pch)+1);
                            strncpy(entry, pch, strlen(pch));
                            rumble_string_lower(entry);
                            cvector_add(blacklist_badhosts, entry);
                            pch = strtok(NULL, " ");
                        }
                    }
                }
                if ( !strcmp(key, "blacklistbymail")) {
                    char* pch = strtok((char*) val," ");
                    while ( pch != NULL ) {
                        if ( strlen(pch) >= 4) {
                            char* entry = calloc(1, strlen(pch)+1);
                            strncpy(entry, pch, strlen(pch));
                            cvector_add(blacklist_baddomains, entry);
                            pch = strtok(NULL, " ");
                        }
                    }
                }
                if ( !strcmp(key, "logfile")) {
                    blacklist_logfile = calloc(1, strlen(val)+1);
                    strcpy((char*)blacklist_logfile, val);
                }
            }
            else {
                fprintf(stderr, "<blacklist> Error: Could not read %s!\n", cfgfile);
                return EXIT_FAILURE;
            }
        }
        free(buffer);
        fclose(config);
    }
    else {
        fprintf(stderr, "<blacklist> Error: Could not read %s!\n", cfgfile);
        //return EXIT_SUCCESS;
    }
    
    // Hook the module to new connections.
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, rumble_blacklist);
    
    // If fake domain check is enabled, hook that one too
    if ( cvector_size(blacklist_baddomains)) {
        rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_CUE_SMTP_MAIL, rumble_blacklist_domains);
    }
    return EXIT_SUCCESS; // Tell rumble that the module loaded okay.
}
