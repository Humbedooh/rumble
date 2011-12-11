/*
 * File: blacklist.c Author: Humbedooh A simple black-listing module for rumble. Created on January 3,
 * 2011, 8:08 P
 */
#include "../../rumble.h"

/* include <Ws2tcpip.h> */
masterHandle* myMaster = 0;
dvector         *blacklist_baddomains;
dvector         *blacklist_badhosts;
dvector         *blacklist_dnsbl;
dvector			*myConfig;
unsigned int    blacklist_spf = 0;
const char      *blacklist_logfile = 0;
char			str_badhosts[2048];
char			str_baddomains[2048];
char			str_dnsbl[2048];
char			str_logfile[512];


rumblemodule_config_struct luaConfig[] = {
	{"BlacklistByHost", 40, "List of servers that are blacklisted from contacting our SMTP server", RCS_STRING, ""},
	{"BlacklistByMail", 40, "List of email domains that are by default invalid as sender addresses", RCS_STRING, ""},
	{"DNSBL", 40, "A list of DNSBL providers to use for querying", RCS_STRING, ""},
	{"EnableSPF", 1, "Should SPF records be checked?", RCS_BOOLEAN, &blacklist_spf},
	{"Logfile", 24, "Optional location of a logfile for blacklist encounters", RCS_STRING, ""},
	{0,0,0,0}
};

static void dvector_splitstring(dvector* vec, const char* string) {
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *pch;
	char *str;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	if (!string) return;
	str = (char*) calloc(1, strlen(string));
	strncpy(str, string, strlen(string));
	pch	= strtok((char *) str, " ");
    while (pch != NULL) {
        if (strlen(pch) >= 3) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char    *entry = (char *) calloc(1, strlen(pch) + 1);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            strncpy(entry, pch, strlen(pch));
            rumble_string_lower(entry);
			dvector_add(vec, entry);
        }
		pch = strtok(NULL, " ");
    }
	free(str);
}
/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_blacklist_domains(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~*/
    dvector_element *el;
    /*~~~~~~~~~~~~~~~~*/

    /* Check against pre-configured list of bad hosts */
    if (blacklist_baddomains->size) {
        el = blacklist_baddomains->first;
        while (el) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char    *badhost = (char *) el->object;
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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

            el = el->next;
        }
    }

    return (RUMBLE_RETURN_OKAY);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_blacklist(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    /* Resolve client address name */
    struct hostent  *client;
    struct in6_addr IP;
    dvector_element *el;
    const char      *addr;
    /*~~~~~~~~~~~~~~~~~~~~*/

    /* Check if the client has been given permission to skip this check by any other modules. */
    if (session->flags & RUMBLE_SMTP_FREEPASS) return (RUMBLE_RETURN_OKAY);
    else
    {
#if !defined(RUMBLE_MSC)

        /* ANSI method */
        inet_pton(session->client->client_info.ss_family, session->client->addr, &IP);
#else
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        struct sockaddr ss;
        int             sslen = sizeof(struct sockaddr);
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
    if (blacklist_badhosts->size) {
        el = blacklist_badhosts->first;
        while (el) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char    *badhost = (char *) el->object;
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            if (strstr(addr, badhost))
            {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
                printf("<blacklist> %s was blacklisted as a bad host name, aborting\n", addr);
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

            el = el->next;
        }
    }

    /* Check against DNS blacklists */
    if (blacklist_dnsbl->size) {
        if (session->client->client_info.ss_family == AF_INET) {

            /*~~~~~~~~~~~~~~~~~~~~~*/
            /* I only know how to match IPv4 DNSBL :/ */
            unsigned int    a,
                            b,
                            c,
                            d;
            char            *dnshost;
            /*~~~~~~~~~~~~~~~~~~~~~*/

            sscanf(session->client->addr, "%3u.%3u.%3u.%3u", &a, &b, &c, &d);
            el = blacklist_dnsbl->first;
            dnshost = (char *) el->object;
            while (dnshost) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                struct hostent  *bl;
                char            *dnsbl = (char *) calloc(1, strlen(dnshost) + strlen(session->client->addr) + 6);
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                sprintf(dnsbl, "%d.%d.%d.%d.%s", d, c, b, a, dnshost);
                bl = gethostbyname(dnsbl);
                if (bl)
                {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
                    printf("<blacklist> %s was blacklisted by %s, closing connection!\n", session->client->addr, dnshost);
#endif
                    if (blacklist_logfile) {

                        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                        FILE    *fp = fopen(blacklist_logfile, "a");
                        char    *mtime;
                        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                        if (fp) {
                            mtime = rumble_mtime();
                            fprintf(fp, "<blacklist>[%s] %s: %s is blacklisted by DNSBL %s.\r\n", mtime, session->client->addr, addr,
                                    dnshost);
                            fclose(fp);
                            free(mtime);
                        }
                    }

                    free(dnsbl);
                    return (RUMBLE_RETURN_FAILURE);
                }   /* Blacklisted, abort the connection! */

                free(dnsbl);
                el = el->next;
                if (el) dnshost = (char *) el->object;
                else dnshost = NULL;
            }
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

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *cfgfile = (char *) calloc(1, 1024);
	const char* entry;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	myMaster = (masterHandle*) master;
    modinfo->title = "Blacklisting module";
    modinfo->description = "Standard blacklisting module for rumble.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";
    blacklist_badhosts = dvector_init();
    blacklist_baddomains = dvector_init();
    blacklist_dnsbl = dvector_init();
    blacklist_spf = 0;
	myConfig = rumble_readconfig("blacklist.conf");
	if (myConfig) {
		memset(str_badhosts, 0, 2048);
		memset(str_baddomains, 0, 2048);
		memset(str_dnsbl, 0, 2048);
		memset(str_logfile, 0, 512);
		// Log file
		blacklist_logfile = rrdict(myConfig, "logfile");
		if (!strcmp(blacklist_logfile, "0")) blacklist_logfile = 0;
		strcpy(str_logfile, blacklist_logfile);
		luaConfig[4].value = (void*) rrdict(myConfig, "logfile");

		// Blacklisted hosts
		entry = rrdict(myConfig, "blacklistbyhost");
		dvector_splitstring(blacklist_badhosts, entry);
		strcpy(str_badhosts, entry);
		luaConfig[0].value = (void*) str_badhosts;

		// Blacklisted domain names
		entry = rrdict(myConfig, "blacklistbymail");
		dvector_splitstring(blacklist_baddomains, entry);
		strcpy(str_baddomains, entry);
		luaConfig[1].value = (void*) str_baddomains;
		
		// DNSBL providers

		entry = rrdict(myConfig, "dnsbl");
		dvector_splitstring(blacklist_dnsbl, entry);
		strcpy(str_dnsbl, entry);
		luaConfig[2].value = (void*) str_dnsbl;
		
		blacklist_spf = atoi(rrdict(myConfig, "enablespf"));
	}

    /* Hook the module to new connections. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, rumble_blacklist);

    /* If fake domain check is enabled, hook that one too */
    if (blacklist_baddomains->size) {
        rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_CUE_SMTP_MAIL, rumble_blacklist_domains);
    }

    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}

rumbleconfig rumble_module_config(const char* key, const char* value) {
	char filename[1024];
	const char* cfgpath;
	FILE* cfgfile;
	
	if (!key) {
		return luaConfig;
	}
	if (!strcmp(key, "BlacklistByHost") && value) {
		strcpy(str_badhosts, value);
		dvector_splitstring(blacklist_badhosts, value);
	}
	if (!strcmp(key, "BlacklistByMail") && value) {
		strcpy(str_baddomains, value);
		dvector_splitstring(blacklist_badhosts, value);
	}
	if (!strcmp(key, "DNSBL") && value) {
		strcpy(str_dnsbl, value);
		dvector_splitstring(blacklist_dnsbl, value);
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
		fprintf(cfgfile, "# Blacklisting configuration for rumble\n\
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
Logfile %s\n", str_badhosts, str_baddomains,blacklist_spf, str_dnsbl, str_logfile);
		fclose(cfgfile);
	}

	return 0;
}


