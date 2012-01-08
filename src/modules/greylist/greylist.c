/*$I0 */

/* File: greylist.c Author: Humbedooh A simple grey-listing module for rumble. Created on Jan */
#include "../../rumble.h"
#include <string.h>
dvector                     *configuration;
int                         GREYLIST_MAX_AGE = 432000;  /* Grey-list records will linger for 5 days. */
int                         GREYLIST_MIN_AGE = 599;     /* Put new triplets on hold for 10 minutes */
int                         GREYLIST_ENABLED = 1;       /* 1 = yes, 0 = no */
masterHandle                *myMaster = 0;
rumblemodule_config_struct  myConfig[] =
{
    { "quarantine", 3, "How long are new email triplets held back (seconds)", RCS_NUMBER, &GREYLIST_MIN_AGE },
    { "linger", 6, "How long should I keep triplets stored? (seconds)", RCS_NUMBER, &GREYLIST_MAX_AGE },
    { "enabled", 1, "Enable mod_greylist?", RCS_BOOLEAN, &GREYLIST_ENABLED },
    { 0, 0, 0, 0 }
};
cvector                     *rumble_greyList;
typedef struct
{
    char    *what;
    time_t  when;
} rumble_triplet;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_greylist(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    address         *recipient;
    char            *block,
                    *tmp,
                    *str;
    time_t          n,
                    now;
    rumble_triplet  *item;
    c_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!GREYLIST_ENABLED) return (RUMBLE_RETURN_OKAY);

    /* First, check if the client has been given permission to skip this check by any other modu */
    if (session->flags & RUMBLE_SMTP_FREEPASS) return (RUMBLE_RETURN_OKAY);

    /* Create the SHA1 hash that corresponds to the triplet. */
    recipient = session->recipients->size ? (address *) session->recipients->first : 0;
    if (!recipient) {
        rumble_debug(NULL, "module", "<greylist> No recipients found! (server bug?)");
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
    tmp = (char *) calloc(1, strlen(session->sender->raw) + strlen(junk) + strlen(block) + 1);
    sprintf(tmp, "%s%s%s", session->sender->raw, junk, block);
    str = rumble_sha256((const unsigned char *) tmp);
    free(tmp);
    free(block);
    n = -1;
    now = time(0);

    /* Run through the list of triplets we have and look for this one. */
    cforeach((rumble_triplet *), item, rumble_greyList, iter) {
        if (!strcmp(item->what, str)) {
            n = now - item->when;
            break;
        }

        /* If the record is too old, delete it from the vector. */
        if ((now - item->when) > GREYLIST_MAX_AGE) {
            cvector_delete(&iter);
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
        cvector_add(rumble_greyList, New);
        n = 0;
    } else free(str);

    /* If the check failed, we tell the client to hold off for 15 minutes. */
    if (n < GREYLIST_MIN_AGE) {
        rcprintf(session, "451 4.7.1 Grey-listed for %u seconds. See http://www.greylisting.org\r\n", GREYLIST_MIN_AGE - n);
        rumble_debug(NULL, "module", "Mail from %s for %s greylisted for %u seconds.\r\n", session->client->addr, junk, GREYLIST_MIN_AGE - n);
        ((rumbleService *) session->_svc)->traffic.rejections++;
        session->client->rejected = 1;
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
    modinfo->description = "Standard greylisting module for rumble.\nAdds a 10 minute quarantine on unknown from-to combinations to prevent spam.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";
    rumble_greyList = cvector_init();
    printf("Reading config...\r\n");
    configuration = rumble_readconfig("greylist.conf");
    printf("done!\r\n");
    GREYLIST_MIN_AGE = atoi(rrdict(configuration, "quarantine"));
    GREYLIST_MAX_AGE = atoi(rrdict(configuration, "linger"));
    GREYLIST_ENABLED = atoi(rrdict(configuration, "enabled"));
    myMaster = (masterHandle *) master;

    /* Hook the module to the DATA command on the SMTP server. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_AFTER + RUMBLE_CUE_SMTP_RCPT, rumble_greylist);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}

/*
 =======================================================================================================================
    rumble_module_config: Sets a config value or retrieves a list of config values.
 =======================================================================================================================
 */
rumbleconfig rumble_module_config(const char *key, const char *value) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    char        filename[1024];
    const char  *cfgpath;
    FILE        *cfgfile;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!key) {
        return (myConfig);
    }

    value = value ? value : "(null)";
    if (!strcmp(key, "quarantine")) GREYLIST_MIN_AGE = atoi(value);
    if (!strcmp(key, "linger")) GREYLIST_MAX_AGE = atoi(value);
    if (!strcmp(key, "enabled")) GREYLIST_ENABLED = atoi(value);
    cfgpath = rumble_config_str(myMaster, "config-dir");
    sprintf(filename, "%s/greylist.conf", cfgpath);
    cfgfile = fopen(filename, "w");
    if (cfgfile) {
        fprintf(cfgfile,
                "# Greylisting configuration. Please use RumbleLua to change these settings.\nQuarantine 	%u\nLinger 		%u\nEnabled 	%u\n", GREYLIST_MIN_AGE,
                GREYLIST_MAX_AGE, GREYLIST_ENABLED);
        fclose(cfgfile);
    }

    return (0);
}
