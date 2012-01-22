#include "../../rumble.h"
#include <string.h>
dvector                     *configuration;
int             Gatekeeper_max_login_attempts =                   3;  // Maximum of concurrent login attempts per account before quarantine
int             Gatekeeper_max_concurrent_threads_per_ip =       25;  // Maximum of concurrent threads per IP
int             Gatekeeper_quarantine_period =                  300;  // Number of seconds to quarantine an IP for too many attempts
int             Gatekeeper_enabled =                              1;  // Enable gatekeper mod?
masterHandle    *myMaster =                                       0;

rumblemodule_config_struct  myConfig[] =
{
    { "loginattempts", 2, "Maximum of concurrent login attempts per IP", RCS_NUMBER, &Gatekeeper_max_login_attempts },
    { "threadsperip", 3, "Maximum of concurrent threads per IP", RCS_NUMBER, &Gatekeeper_max_concurrent_threads_per_ip },
    { "quarantine", 3, "Number of seconds to quarantine an IP for too many attempts", RCS_NUMBER, &Gatekeeper_quarantine_period },
    { "enabled", 1, "Enable mod_greylist?", RCS_BOOLEAN, &Gatekeeper_enabled },
    { 0, 0, 0, 0 }
};
cvector                     *gatekeeper_login_list, *gatekeeper_connection_list;
typedef struct
{
    char    ip[66];
    int     connections;
} gatekeeper_connection;

typedef struct {
    char ip[66];
    int tries;
    time_t lastAttempt;
    char quarantined;
} gatekeeper_login_attempt;


/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_gatekeeper_accept(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    gatekeeper_login_attempt* lentry;
    gatekeeper_connection* centry;
    time_t          now;
    c_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!Gatekeeper_enabled) return (RUMBLE_RETURN_OKAY);

    
    
    // First, check the guys quarantined through bad login attempts
    cforeach((gatekeeper_login_attempt*), lentry, gatekeeper_login_list, iter) {
        if (!strcmp(lentry->ip, session->client->addr)) {
            if (lentry->quarantined == 1) {
                now = time(0);
                if ((now - lentry->lastAttempt) > Gatekeeper_quarantine_period) {
                    lentry->quarantined = 0;
                    lentry->tries = 0;
                    break;
                }
                else {
                    rcprintf(session, "Too many bad logins! Quarantined for %u seconds.\r\n", Gatekeeper_quarantine_period);
                    return RUMBLE_RETURN_FAILURE;
                }
            }
        }
    }
    
    // Then, let's check if the IP is using too many threads
    
    
    return (RUMBLE_RETURN_OKAY);
}


ssize_t rumble_gatekeeper_close(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    gatekeeper_connection* centry;
    c_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!Gatekeeper_enabled) return (RUMBLE_RETURN_OKAY);
    return RUMBLE_RETURN_OKAY;
}


ssize_t rumble_gatekeeper_auth(sessionHandle *session, const char *OK) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    gatekeeper_login_attempt* entry;
    time_t          now;
    c_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!Gatekeeper_enabled) return (RUMBLE_RETURN_OKAY);
    // Was the login OK? If so, let's delete any counters we have
    if (OK) {
        cforeach((gatekeeper_login_attempt*), entry, gatekeeper_login_list, iter) {
            if (!strcmp(entry->ip, session->client->addr)) {
                cvector_delete(&iter);
                free(entry);
                break;
            }
        }
    }
    // Login went bad, let's write that down!
    else {
        int found = 0;
        rumble_debug(myMaster, "module", "bad login attempt, writing it down");
        cforeach((gatekeeper_login_attempt*), entry, gatekeeper_login_list, iter) {
            if (!strcmp(entry->ip, session->client->addr)) {
                entry->tries++;
                entry->lastAttempt = time(0);
                if (entry->tries >= Gatekeeper_max_login_attempts) entry->quarantined = 1;
                found = 1;
                rcprintf(session, "Too many login attempts detected, quarantined for %u seconds!\r\n", Gatekeeper_quarantine_period);
                return RUMBLE_RETURN_FAILURE;
            }
        }
        if (!found) {
            entry = (gatekeeper_login_attempt*) malloc(sizeof(gatekeeper_login_attempt));
            entry->tries = 1;
            entry->lastAttempt = time(0);
            entry->quarantined = 0;
            strcpy(entry->ip, session->client->addr);
            cvector_add(gatekeeper_login_list, entry);
        }
    }

    return (RUMBLE_RETURN_OKAY);
}
/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {
    modinfo->title = "Gatekeeper module";
    modinfo->description = "This module controls how many login attempts and concurrent connections each client is allowed.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";
    printf("Reading config...\r\n");
    configuration = rumble_readconfig("gatekeeper.conf");
    printf("done!\r\n");
    Gatekeeper_max_login_attempts = atoi(rrdict(configuration, "loginattempts"));
    Gatekeeper_max_concurrent_threads_per_ip = atoi(rrdict(configuration, "threadsperip"));
    Gatekeeper_quarantine_period = atoi(rrdict(configuration, "quarantine"));
    Gatekeeper_enabled = atoi(rrdict(configuration, "enabled"));
    myMaster = (masterHandle *) master;
    
    gatekeeper_login_list = cvector_init();
    gatekeeper_connection_list = cvector_init();

    
    // Hook onto any new incoming connections on SMTP, IMAP and POP3
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, rumble_gatekeeper_accept);
    rumble_hook_function(master, RUMBLE_HOOK_IMAP + RUMBLE_HOOK_ACCEPT, rumble_gatekeeper_accept);
    rumble_hook_function(master, RUMBLE_HOOK_POP3 + RUMBLE_HOOK_ACCEPT, rumble_gatekeeper_accept);
    
    /* Hook the module to the LOGIN command on the SMTP and IMAP server. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_AFTER + RUMBLE_CUE_SMTP_AUTH, rumble_gatekeeper_auth);
    rumble_hook_function(master, RUMBLE_HOOK_IMAP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_AFTER + RUMBLE_CUE_IMAP_AUTH, rumble_gatekeeper_auth);
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
    if (!strcmp(key, "loginattempts")) Gatekeeper_max_login_attempts = atoi(value);
    if (!strcmp(key, "threadsperip")) Gatekeeper_max_concurrent_threads_per_ip = atoi(value);
    if (!strcmp(key, "quarantine")) Gatekeeper_quarantine_period = atoi(value);
    if (!strcmp(key, "enabled")) Gatekeeper_enabled = atoi(value);
            
            
    cfgpath = rumble_config_str(myMaster, "config-dir");
    sprintf(filename, "%s/gatekeeper.conf", cfgpath);
    cfgfile = fopen(filename, "w");
    if (cfgfile) {
        fprintf(cfgfile,
                "\
# Gatekeeper configuration. Please use RumbleLua to change these settings.\n\
LoginAttempts 	%u\n\
ThreadsPerIP 		%u\n\
Quarantine              %u\n\
Enabled          	%u\n", \
                Gatekeeper_max_login_attempts, Gatekeeper_max_concurrent_threads_per_ip, Gatekeeper_quarantine_period, Gatekeeper_enabled);
        fclose(cfgfile);
    }

    return (0);
}
