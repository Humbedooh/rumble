/*
 * File: greylist.c Author: Humbedooh A simple grey-listing module for rumble.
 * Created on January 3,
 */
#include <string.h>
#include "../../rumble.h"
#define GREYLIST_MAX_AGE    172800  /* Grey-list records will linger for 48 hours. */
#define GREYLIST_MIN_AGE    900     /* Put new triplets on hold for 15 minutes */
dvector rumble_greyList;
typedef struct
{
    char    *what;
    time_t  when;
} rumble_triplet;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_greylist(sessionHandle *session) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    address         *recipient;
    char            *block,
                    *tmp,
                    *str;
    time_t          n,
                    now;
    rumble_triplet  *item;
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * First, check if the client has been given permission to skip this check by any
     * other modules
     */
    if (session->flags & RUMBLE_SMTP_FREEPASS) return (RUMBLE_RETURN_OKAY);

    /* Create the SHA1 hash that corresponds to the triplet. */
    recipient = (address *) session->recipients->last;
    if (!recipient) {
        printf("<greylist> No recipients found! (server bug?)\n");
        return (RUMBLE_RETURN_FAILURE);
    }

    /* Truncate the IP address to either /24 for IPv4 or /64 for IPv6 */
    block = (char *) calloc(1, 20);
    if (!strchr(session->client->addr, ':')) {

        /*~~~~~~~~~~~~~~*/
        /* IPv4 */
        unsigned int    a,
                        b,
                        c;
        /*~~~~~~~~~~~~~~*/

        sscanf(session->client->addr, "%3u.%3u.%3u", &a, &b, &c);
        sprintf(block, "%03u.%03u.%03u", a, b, c);
    } else strncpy(block, session->client->addr, 19);   /* IPv6 */
    tmp = (char *) calloc(1, strlen(session->sender->raw) + strlen(recipient->raw) + strlen(block) + 1);
    sprintf(tmp, "%s%s%s", session->sender->raw, recipient->raw, block);
    str = rumble_sha160((const unsigned char *) tmp);
    free(tmp);
    free(block);
    n = -1;
    now = time(0);

    /* Run through the list of triplets we have and look for this one. */
    foreach((rumble_triplet *), item, &rumble_greyList, iter) {
        if (!strcmp(item->what, str)) {
            n = now - item->when;
            break;
        }

        /* If the record is too old, delete it from the vector. */
        if ((now - item->when) > GREYLIST_MAX_AGE) {
            dvector_delete(&iter);
            free(item->what);
            free(item);
        }
    }

    /* If no such triplet, create one and add it to the vector. */
    if (n == -1) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        rumble_triplet  *New = (rumble_triplet *) malloc(sizeof(rumble_triplet));
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        New->what = str;
        New->when = now;
        dvector_add(&rumble_greyList, New);
        n = 0;
    } else free(str);

    /* If the check failed, we tell the client to hold off for 15 minutes. */
    if (n < GREYLIST_MIN_AGE) {
        rcprintf(session, "451 4.7.1 Grey-listed for %u seconds. See http://www.greylisting.org\r\n", GREYLIST_MIN_AGE - n);
        return (RUMBLE_RETURN_IGNORE);  /* Tell rumble to ignore the command quietly. */
    }

    /* Otherwise, we just return with EXIT_SUCCESS and let the server continue. */
    return (RUMBLE_RETURN_OKAY);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {
    modinfo->title = "Greylisting module";
    modinfo->description = "Standard greylisting module for rumble.";

    /* Hook the module to the DATA command on the SMTP server. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_AFTER + RUMBLE_CUE_SMTP_RCPT, rumble_greylist);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
