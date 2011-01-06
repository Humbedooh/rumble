/* 
 * File:   whitelist.c
 * Author: Humbedooh
 * 
 * A simple white-listing module for rumble.
 *
 * Created on January 3, 2011, 8:08 PM
 */

#include <string.h>
#include "../../rumble.h"
cvector* rumble_whiteList;


ssize_t rumble_whitelist(sessionHandle* session) {
    // Make our own copy of the IP with an added dot at the end.
    char* ip = malloc(strlen(session->client->addr)+2);
    sprintf(ip, "%s.", session->client->addr);
    // Go through the list of white-listed spaces and see what we find.
    const char* addr;
    for ( addr = (const char*) cvector_first(rumble_whiteList); addr != NULL; addr = (const char*) cvector_next(rumble_whiteList) ) {
        if ( !strncmp(addr, ip, strlen(addr))) {
            session->flags |= RUMBLE_SMTP_WHITELIST; // Set the whitelist flag if the client matches a range.
            printf("%s is whitelisted!\n", session->client->addr);
            break;
        }
    }
    // Return with EXIT_SUCCESS and let the server continue.
    return EXIT_SUCCESS;
}

int rumble_module_init(masterHandle* master) {
    rumble_whiteList = cvector_init();
    FILE* config = fopen("config/whitelist.conf", "r");
    if ( config ) {
        char* buffer = malloc(200);
        int p = 0;
        char byte;
        while (!feof(config)) {
            memset(buffer, 0, 200);
            fgets(buffer, 200,config);
            if ( !ferror(config) ) {
                char* address = calloc(1, 46);
                sscanf(buffer, "%46[^# \t\r\n]", address );
                if ( strlen(address) ) {
                    char* el = calloc(1, strlen(address)+2);
                    sprintf(el, "%s.", address); // add a trailing dot for security measures.
                    cvector_add(rumble_whiteList, el);
                }
            }
            else {
                perror("<whitelist> Error: Could not read config/whitelist.conf");
                exit(EXIT_FAILURE);
            }
        }
        free(buffer);
        fclose(config);
    }
    else {
        perror("<whitelist> Error: Could not read config/whitelist.conf");
        exit(EXIT_FAILURE);
    }
    
    // Hook the module to new connections.
    rumble_hook_on_smtp_accept(master, rumble_whitelist);
    return EXIT_SUCCESS; // Tell rumble that the module loaded okay.
}
