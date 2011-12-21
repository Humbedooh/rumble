/*
 * File: whitelist.c Author: Humbedooh A simple white-listing module for rumble. Created on January 3,
 * 2011, 8:0
 */
#include <string.h>
#include "../../rumble.h"
cvector *rumble_whiteList;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_whitelist(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    /* Make our own copy of the IP with an added dot at the end. */
    const char  *addr;
    char        *ip = (char *) malloc(strlen(session->client->addr) + 2);
    c_iterator  iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sprintf(ip, "%s.", session->client->addr);

    /* Go through the list of white-listed spaces and see what we find. */
    cforeach((const char *), addr, rumble_whiteList, iter) {
        if (!strncmp(addr, ip, strlen(addr))) {
            session->flags |= RUMBLE_SMTP_WHITELIST;    /* Set the whitelist flag if the client matches a range. */
            break;
        }
    }

    /* Return with RUMBLE_RETURN_OKAY and let the server continue. */
    return (RUMBLE_RETURN_OKAY);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    FILE    *config;
    char    *cfgfile = (char *) calloc(1, 1024);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    modinfo->title = "Whitelisting module";
    modinfo->description = "Standard whitelisting module for rumble. Allows SMTP traffic from pre-defined known email servers to pass through without having to go through greylisting first.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";
    rumble_whiteList = cvector_init();
    sprintf(cfgfile, "%s/whitelist.conf", ((masterHandle *) master)->cfgdir);
    config = fopen(cfgfile, "r");
    free(cfgfile);
    if (config) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        char    *buffer = (char *) malloc(200);
        int     p = 0;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        while (!feof(config)) {
            memset(buffer, 0, 200);
            fgets(buffer, 200, config);
            if (!ferror(config)) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                char    *address = (char *) calloc(1, 46);
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                sscanf(buffer, "%46[^# \t\r\n]", address);
                if (strlen(address)) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    char    *el = (char *) calloc(1, strlen(address) + 2);
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    sprintf(el, "%s.", address);    /* add a trailing dot for security measures. */
                    cvector_add(rumble_whiteList, el);
                }

                free(address);
            } else {
                perror("<whitelist> Error: Could not read config/whitelist.conf");
                exit(EXIT_FAILURE);
            }
        }

        free(buffer);
        fclose(config);
    } else {
        perror("<whitelist> Error: Could not read config/whitelist.conf");
        exit(EXIT_FAILURE);
    }

    /* Hook the module to new connections. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, rumble_whitelist);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
