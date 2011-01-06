/* 
 * File:   greylist.c
 * Author: Humbedooh
 * 
 * A simple grey-listing module for rumble.
 *
 * Created on January 3, 2011, 8:08 PM
 */

#include <string.h>
#include "../../rumble.h"
#define GREYLIST_MAX_AGE 172800 // Grey-list records will linger for 48 hours.
#define GREYLIST_MIN_AGE 900 // Put new triplets on hold for 15 minutes
cvector rumble_greyList;

typedef struct {
    char*               what;
    time_t              when;
} rumble_triplet;


ssize_t rumble_greylist(sessionHandle* session) {
    // First, check if the client has been given permission to skip this check by any other modules.
    if ( session->flags & RUMBLE_SMTP_FREEPASS ) return EXIT_SUCCESS;
    
    // Create the SHA1 hash that corresponds to the triplet.
    char* tmp = malloc(strlen(session->sender.raw) + strlen(session->recipient.raw) + strlen(session->client->addr) + 1);
    sprintf(tmp, "%s%s%s", session->sender.raw, session->recipient.raw, session->client->addr);
    char* str = rumble_sha160(tmp);
    free(tmp);
    time_t n = -1;
    time_t now = time(0);
    rumble_triplet* item;
    // Run through the list of triplets we have and look for this one.
    for (item = (rumble_triplet*) cvector_first(&rumble_greyList); item != NULL; item = (rumble_triplet*) cvector_next(&rumble_greyList)) {
        if ( !strcmp(item->what, str)) { 
            n = now - item->when;
            break; 
        }
        // If the record is too old, delete it from the vector.
        if ( (now - item->when) > GREYLIST_MAX_AGE ) {
            cvector_delete(&rumble_greyList);
            free(item->what);
            free(item);
        }
    }
    // If no such triplet, create one and add it to the vector.
    if ( n == -1 ) {
        rumble_triplet* new = malloc(sizeof(rumble_triplet));
        new->what = str;
        new->when = now;
        cvector_add(&rumble_greyList, new);
    }
    else free(str);
    // If the check failed, we tell the client to hold off for 15 minutes.
    if ( n < GREYLIST_MIN_AGE ) {
        rumble_comm_send(session, "451 Grey-listed for 15 minutes.\r\n");
        return EXIT_FAILURE; // Tell rumble to halt the transaction quietly.
    }
    // Otherwise, we just return with EXIT_SUCCESS and let the server continue.
    return EXIT_SUCCESS;
}

int rumble_module_init(masterHandle* master) {
    // Hook the module to the DATA command on the SMTP server.
    rumble_hook_on_smtp_cmd(master, RUMBLE_CUE_SMTP_DATA, rumble_greylist);
    return EXIT_SUCCESS; // Tell rumble that the module loaded okay.
}
