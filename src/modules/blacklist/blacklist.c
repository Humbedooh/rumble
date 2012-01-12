/*
 * File: blacklist.c Author: Humbedooh A simple black-listing module for rumble. Created on January 3,
 * 2011, 8:08
 */
#include "../../rumble.h"
typedef struct
{
    time_t          when;
    unsigned int    IP[4];
} blackListEntry;

/* include <Ws2tcpip.h> */
masterHandle                *myMaster = 0;
rumble_args                 *blacklist_baddomains;
rumble_args                 *blacklist_badhosts;
rumble_args                 *blacklist_dnsbl;
cvector                     *fastList;
dvector                     *myConfig;
unsigned int                blacklist_spf = 0;
const char                  *blacklist_logfile = 0;
char                        str_badhosts[2048];
char                        str_baddomains[2048];
char                        str_dnsbl[2048];
char                        str_logfile[512];
rumblemodule_config_struct  luaConfig[] =
{
    { "BlacklistByHost", 40, "List of servers that are blacklisted from contacting our SMTP server", RCS_STRING, "" },
    { "BlacklistByMail", 40, "List of email domains that are by default invalid as sender addresses", RCS_STRING, "" },
    { "DNSBL", 40, "A list of DNSBL providers to use for querying", RCS_STRING, "" },
    { "EnableSPF", 1, "Should SPF records be checked?", RCS_BOOLEAN, &blacklist_spf },
    { "Logfile", 24, "Optional location of a logfile for blacklist encounters", RCS_STRING, "" },
    { 0, 0, 0, 0 }
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_blacklist_domains(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~*/
    int     i = 0;
    char    *badhost = 0;
    /*~~~~~~~~~~~~~~~~~*/

    /* Check against pre-configured list of bad hosts */
    for (i = 0; i < blacklist_baddomains->argc; i++) {
        badhost = blacklist_baddomains->argv[i];
        if (!strcmp(session->sender->domain, badhost))
        {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
            printf("<blacklist> %s was blacklisted as a bad domain, aborting\n", badhost);
#endif
            rumble_comm_send(session, "530 Sender domain has been blacklisted.\r\n");
            if (blacklist_logfile) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                FILE    *fp = fopen(blacklist_logfile, "a");
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                if (fp) {

                    /*~~~~~~~~~~~~*/
                    time_t  rawtime;
                    /*~~~~~~~~~~~~*/

                    time(&rawtime);
                    fprintf(fp, "<blacklist>[%s] %s: Attempt to use %s as sender domain.\r\n", ctime(&rawtime), session->client->addr,
                            badhost);
                    fclose(fp);
                }
            }

            return (RUMBLE_RETURN_IGNORE);
        }
    }

    return (RUMBLE_RETURN_OKAY);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_blacklist(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    /* Resolve client address name */
    struct hostent  *client;
    struct in6_addr IP;
    const char      *addr;
    blackListEntry  *entry;
    c_iterator      iter;
    unsigned int    a,
                    b,
                    c,
                    d;
    char            *badhost;
    int             i;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    /* Check if the client has been given permission to skip this check by any other modules. */
    if (session->flags & RUMBLE_SMTP_FREEPASS) return (RUMBLE_RETURN_OKAY);
    else
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef RUMBLE_MSC
        struct sockaddr ss;
        int             sslen = sizeof(struct sockaddr);
#endif
        int             x = 0;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        sscanf(session->client->addr, "%3u.%3u.%3u.%3u", &a, &b, &c, &d);

        /* Check against the fast list of already encountered spammers */
        cforeach((blackListEntry *), entry, fastList, iter) {
            x++;
            printf("checking fl rec no. %u\n", x);
            if (entry->IP[0] == a && entry->IP[1] == b && entry->IP[2] == c && entry->IP[3] == d) {

                /*~~~~~~~~~~~~~~~~~~~~~*/
                time_t  now = time(NULL);
                /*~~~~~~~~~~~~~~~~~~~~~*/

                if (now - entry->when > 86400) {
                    cvector_delete(&iter);
                    free(entry);
                } else {
                    return (RUMBLE_RETURN_FAILURE);
                    rumble_debug((masterHandle *) session->_master, "smtp", "<blacklist> %s is listed in the fast list as a spam host.",
                                 session->client->addr);
                }
            }
        }

#if !defined(RUMBLE_MSC)

        /* ANSI method */
        inet_pton(session->client->client_info.ss_family, session->client->addr, &IP);
#else

        /* Windows method */
        WSAStringToAddressA(session->client->addr, session->client->client_info.ss_family, NULL, (struct sockaddr *) &ss, &sslen);
        IP = ((struct sockaddr_in6 *) &ss)->sin6_addr;
#endif
        client = gethostbyaddr((char *) &IP, (session->client->client_info.ss_family == AF_INET) ? 4 : 16,
                               session->client->client_info.ss_family);
        if (!client) return (RUMBLE_RETURN_IGNORE);
        addr = (const char *) client->h_name;
        rumble_string_lower((char *) addr);
    }

    /* Check against pre-configured list of bad hosts */
    for (i = 0; i < blacklist_badhosts->argc; i++) {
        badhost = blacklist_badhosts->argv[i];
        if (strstr(addr, badhost))
        {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
            rumble_debug((masterHandle *) session->_master, "smtp", "<blacklist> %s was blacklisted as a bad host name, aborting\n", addr);
#endif
            if (blacklist_logfile) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                FILE    *fp = fopen(blacklist_logfile, "w+");
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                if (fp) {

                    /*~~~~~~~~~~~~*/
                    time_t  rawtime;
                    /*~~~~~~~~~~~~*/

                    time(&rawtime);
                    fprintf(fp, "<blacklist>[%s] %s: %s is blacklisted as a bad host.\r\n", ctime(&rawtime), session->client->addr, addr);
                    fclose(fp);
                }
            }

            return (RUMBLE_RETURN_FAILURE);
        }
    }

    /* Check against DNS blacklists */
    if (session->client->client_info.ss_family == AF_INET) {

        /*~~~~~~~~~~~~~~~~~~~~~~~*/
        /* I only know how to match IPv4 DNSBL :/ */
        char            *dnshost;
        struct hostent  *bl;
        blackListEntry  *entry = 0;
        char            *dnsbl;
        /*~~~~~~~~~~~~~~~~~~~~~~~*/

        for (i = i; i < blacklist_dnsbl->argc; i++) {
            dnshost = (char *) blacklist_dnsbl->argv[i];
            dnsbl = (char *) calloc(1, strlen(dnshost) + strlen(session->client->addr) + 6);
            sprintf(dnsbl, "%d.%d.%d.%d.%s", d, c, b, a, dnshost);
            bl = gethostbyname(dnsbl);
            if (bl)
            {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
                printf("<blacklist> %s was blacklisted by %s, closing connection!\n", session->client->addr, dnshost);
#endif
                printf("Adding entry %u.%u.%u.%u to fl\n", a, b, c, d);
                entry = (blackListEntry *) malloc(sizeof(blackListEntry));
                entry->when = time(NULL);
                entry->IP[0] = a;
                entry->IP[1] = b;
                entry->IP[2] = c;
                entry->IP[3] = d;
                cvector_add(fastList, entry);
                if (blacklist_logfile) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    FILE    *fp = fopen(blacklist_logfile, "a");
                    char    *mtime;
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    if (fp) {
                        mtime = rumble_mtime();
                        rumble_debug(myMaster, "<blacklist>[%s] %s: %s is blacklisted by DNSBL %s.\r\n", mtime, session->client->addr, addr, dnshost);
                        fprintf(fp, "<blacklist>[%s] %s: %s is blacklisted by DNSBL %s.\r\n", mtime, session->client->addr, addr, dnshost);
                        fclose(fp);
                        free(mtime);
                    }
                }

                free(dnsbl);
                return (RUMBLE_RETURN_FAILURE);
            }   /* Blacklisted, abort the connection! */

            free(dnsbl);
        }
    }

    /* Return with EXIT_SUCCESS and let the server continue. */
    return (RUMBLE_RETURN_OKAY);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {

    /*~~~~~~~~~~~~~~~*/
    const char  *entry;
    /*~~~~~~~~~~~~~~~*/

    myMaster = (masterHandle *) master;
    modinfo->title = "Blacklisting module";
    modinfo->description = "Standard blacklisting module for rumble.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";
    fastList = cvector_init();
    blacklist_spf = 0;
    myConfig = rumble_readconfig("blacklist.conf");
    if (myConfig) {
        memset(str_badhosts, 0, 2048);
        memset(str_baddomains, 0, 2048);
        memset(str_dnsbl, 0, 2048);
        memset(str_logfile, 0, 512);

        /* Log file */
        blacklist_logfile = rrdict(myConfig, "logfile");
        if (!strcmp(blacklist_logfile, "0")) blacklist_logfile = 0;
        strcpy(str_logfile, blacklist_logfile);
        luaConfig[4].value = (void *) rrdict(myConfig, "logfile");

        /* Blacklisted hosts */
        entry = rrdict(myConfig, "blacklistbyhost");
        blacklist_badhosts = rumble_read_words(entry);
        strcpy(str_badhosts, entry);
        luaConfig[0].value = (void *) str_badhosts;

        /* Blacklisted domain names */
        entry = rrdict(myConfig, "blacklistbymail");
        blacklist_baddomains = rumble_read_words(entry);
        strcpy(str_baddomains, entry);
        luaConfig[1].value = (void *) str_baddomains;

        /* DNSBL providers */
        entry = rrdict(myConfig, "dnsbl");
        blacklist_dnsbl = rumble_read_words(entry);
        strcpy(str_dnsbl, entry);
        luaConfig[2].value = (void *) str_dnsbl;
        blacklist_spf = atoi(rrdict(myConfig, "enablespf"));
    }

    /* Hook the module to new connections. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, rumble_blacklist);

    /* If fake domain check is enabled, hook that one too */
    if (blacklist_baddomains->argc > 0) {
        rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_CUE_SMTP_MAIL, rumble_blacklist_domains);
    }

    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumbleconfig rumble_module_config(const char *key, const char *value) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    char        filename[1024];
    const char  *cfgpath;
    FILE        *cfgfile;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!key) {
        return (luaConfig);
    }

    if (!strcmp(key, "BlacklistByHost") && value) {
        strcpy(str_badhosts, value);
        blacklist_badhosts = rumble_read_words(value);
    }

    if (!strcmp(key, "BlacklistByMail") && value) {
        strcpy(str_baddomains, value);
        blacklist_baddomains = rumble_read_words(value);
    }

    if (!strcmp(key, "DNSBL") && value) {
        strcpy(str_dnsbl, value);
        blacklist_dnsbl = rumble_read_words(value);
    }

    if (!strcmp(key, "EnableSPF")) {
        blacklist_spf = atoi(value);
    }

    if (!strcmp(key, "Logfile") && value) {
        strcpy(str_logfile, value);
        if (!strlen(value)) blacklist_logfile = 0;
        else blacklist_logfile = str_logfile;
    }

    cfgpath = rumble_config_str(myMaster, "config-dir");
    sprintf(filename, "%s/blacklist.conf", cfgpath);
    cfgfile = fopen(filename, "w");
    if (cfgfile) {
        fprintf(cfgfile,
                "# Blacklisting configuration for rumble\n\
\n\
# BlacklistByHost: Contains a list of host addresses commonly used by spammers.\n\
BlacklistByHost     %s\n\
\n\
# BlacklistByMail: Contains a list of fake domains commonly used as senders of spam.\n\
BlacklistByMail     %s\n\
\n\
# EnableSPF: Set to 1 to enable checking SPF records for received email or 0 to disable this feature.\n\
EnableSPF           %u\n\
\n\
# DNSBL: Contains a list of DNS BlackList operators to query for information on the client connecting to the service.\n\
DNSBL               %s\n\
\n\
# Logfile: If set, all blacklisting activity will be written to this log file.\n\
Logfile %s\n",
            str_badhosts, str_baddomains, blacklist_spf, str_dnsbl, str_logfile);
        fclose(cfgfile);
    }

    return (0);
}
