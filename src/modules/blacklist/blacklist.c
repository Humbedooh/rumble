/*
 * File: blacklist.c Author: Humbedooh A simple black-listing module for rumble.
 * Created on January 3, 2011, 8:08 P
 */
#include "../../rumble.h"

/* include <Ws2tcpip.h> */
dvector         *blacklist_baddomains;
dvector         *blacklist_badhosts;
dvector         *blacklist_dnsbl;
unsigned int    blacklist_spf = 0;
const char      *blacklist_logfile = 0;

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

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    /* Resolve client address name */
    struct hostent          *client;
    struct in6_addr         IP;
    dvector_element         *el;
    const char              *addr;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * Check if the client has been given permission to skip this check by any other
     * modules.
     */
    if (session->flags & RUMBLE_SMTP_FREEPASS) return (RUMBLE_RETURN_OKAY);
#ifndef RUMBLE_MSC

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
    FILE    *config;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    modinfo->title = "Blacklisting module";
    modinfo->description = "Standard blacklisting module for rumble.";
    modinfo->author = "Humbedooh (humbedooh@users.sf.net)";
    blacklist_badhosts = dvector_init();
    blacklist_baddomains = dvector_init();
    blacklist_dnsbl = dvector_init();
    sprintf(cfgfile, "%s/blacklist.conf", ((masterHandle *) master)->cfgdir);
    config = fopen(cfgfile, "r");
    if (config) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        char    *buffer = (char *) malloc(4096);
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        while (!feof(config)) {
            memset(buffer, 0, 4096);
            if (!fgets(buffer, 4096, config)) continue;
            if (!ferror(config)) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                char    *key = (char *) calloc(1, 100);
                char    *val = (char *) calloc(1, 3900);
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                sscanf(buffer, "%100[^# \t\r\n] %3900[^\r\n]", key, val);
                rumble_string_lower(key);
                if (!strcmp(key, "dnsbl")) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    char    *pch = strtok((char *) val, " ");
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    while (pch != NULL) {
                        if (strlen(pch) >= 4) {

                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                            char    *entry = (char *) calloc(1, strlen(pch) + 1);
                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                            strncpy(entry, pch, strlen(pch));
                            dvector_add(blacklist_dnsbl, entry);
                            pch = strtok(NULL, " ");
                        }
                    }
                }

                if (!strcmp(key, "enablespf")) blacklist_spf = atoi(val);
                if (!strcmp(key, "blacklistbyhost")) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    char    *pch = strtok((char *) val, " ");
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    while (pch != NULL) {
                        if (strlen(pch) >= 4) {

                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                            char    *entry = (char *) calloc(1, strlen(pch) + 1);
                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                            strncpy(entry, pch, strlen(pch));
                            rumble_string_lower(entry);
                            dvector_add(blacklist_badhosts, entry);
                            pch = strtok(NULL, " ");
                        }
                    }
                }

                if (!strcmp(key, "blacklistbymail")) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    char    *pch = strtok((char *) val, " ");
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    while (pch != NULL) {
                        if (strlen(pch) >= 4) {

                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                            char    *entry = (char *) calloc(1, strlen(pch) + 1);
                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                            strncpy(entry, pch, strlen(pch));
                            dvector_add(blacklist_baddomains, entry);
                            pch = strtok(NULL, " ");
                        }
                    }
                }

                if (!strcmp(key, "logfile")) {
                    blacklist_logfile = calloc(1, strlen(val) + 1);
                    strcpy((char *) blacklist_logfile, val);
                }
            } else {
                fprintf(stderr, "<blacklist> Error: Could not read %s!\n", cfgfile);
                return (EXIT_FAILURE);
            }
        }

        free(buffer);
        fclose(config);
    } else {
        fprintf(stderr, "<blacklist> Error: Could not read %s!\n", cfgfile);
        return (EXIT_SUCCESS);
    }

    /* Hook the module to new connections. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, rumble_blacklist);

    /* If fake domain check is enabled, hook that one too */
    if (blacklist_baddomains->size) {
        rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_CUE_SMTP_MAIL, rumble_blacklist_domains);
    }

    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
