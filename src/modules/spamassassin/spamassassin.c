/* File: spamassassin.c Author: Humbedooh Created on 13. june 2011, 20:11 */
#include "../../rumble.h"
#include "../../comm.h"

/* include <Ws2tcpip.h> */
dvector                     *sa_config;
int                         sa_spamscore, sa_modifyifspam, sa_modifyifham, sa_deleteifspam, sa_port, sa_usedaemon, sa_enabled;
char                        sa_host[512];
char                        sa_exec[512];
masterHandle                *myMaster;
rumblemodule_config_struct  myConfig[] =
{
    { "SpamScore", 2, "At which score should emails be considered spam?", RCS_NUMBER, &sa_spamscore },
    { "ModifyIfSpam", 2, "Should SpamAssassin modify message headers if spam?", RCS_BOOLEAN, &sa_modifyifspam },
    { "ModifyIfHam", 2, "Should SpamAssassin modify message headers if ham (non-spam)?", RCS_BOOLEAN, &sa_modifyifham },
    { "DeleteIfSpam", 2, "Should SpamAssassin delete spam?", RCS_BOOLEAN, &sa_deleteifspam },
    { "UseDaemon", 2, "Should we try the SA daemon process?", RCS_BOOLEAN, &sa_usedaemon },
    { "HostName", 16, "If using daemon, which port is it on?", RCS_STRING, &sa_host },
    { "PortNumber", 3, "If using daemon, which port is it on?", RCS_NUMBER, &sa_port },
    { "SpamExecutable", 32, "If not using the daemon, enter the name of the SpamAssassin executable to run instead", RCS_STRING, &sa_exec },
    { "Enabled", 2, "Should SpamAssassin be enabled on rumble?", RCS_BOOLEAN, &sa_enabled },
    { 0, 0, 0, 0 }
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t sa_check(sessionHandle *session, const char *filename) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    buffer[2001],
            *line;
    FILE    *fp;
    size_t  fsize,
            bread;
    ssize_t ret = RUMBLE_RETURN_OKAY;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!sa_enabled) return (RUMBLE_RETURN_OKAY);
    printf("[SA]: SpamAssassin plugin is now checking %s...\n", filename ? filename : "??");
    rcsend(session, "250-Checking message...\r\n");
    fp = fopen(filename, "rb");
    if (!fp) {
        perror("Couldn't open file!");
        return (RUMBLE_RETURN_OKAY);
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    rewind(fp);

    /* Use the spamd server? */
    if (sa_usedaemon) {

        /*~~~~~~~~~~~~~~*/
        sessionHandle   s;
        clientHandle    c;
        /*~~~~~~~~~~~~~~*/

        s.client = &c;
        s._svc = 0;
        printf("[SA]: Connecting to spamd <%s>...\n", sa_host);
        c.socket = comm_open((masterHandle *) session->_master, sa_host, sa_port);
        c.tls = 0;
        c.recv = 0;
        c.send = 0;
        FD_ZERO(&c.fd);
        FD_SET(c.socket, &c.fd);
        if (c.socket) {
            printf("Connected to spamd, sending request\n");
            if (sa_modifyifham || (sa_modifyifspam and!sa_deleteifspam)) rcsend(&s, "PROCESS SPAMC/1.5\r\n");
            else rcsend(&s, "CHECK SPAMC/1.5\r\n");
            rcprintf(&s, "Content-length: %u\r\n\r\n", fsize);
            while (!feof(fp) && fp) {
                memset(buffer, 0, 2000);
                bread = fread(buffer, 1, 2000, fp);
                send(c.socket, buffer, (int) bread, 0);
            }

            fclose(fp);
            if (c.socket) {

                /*~~~~~~~~~*/
                int x = 0,
                    spam = 0;
                /*~~~~~~~~~*/

                printf("[SA] Recieving response...\n");
                line = rcread(&s);
                if (line) {
                    if (strstr(line, "EX_OK")) {
                        while (strlen(line) > 2) {
                            free(line);
                            line = rcread(&s);
                            if (!line) break;
                            if (strstr(line, "Spam: True")) {
                                spam = 1;
                            }

                            if (strstr(line, "Spam: False")) {
                                spam = 0;
                            }
                        }

                        free(line);
                        printf("[SA]: The message is %s!\n", spam ? "SPAM" : "not spam");
                        if (spam and sa_deleteifspam) {
                            printf("[SA] Deleting %s\n", filename);
                            unlink(filename);
                            ret = RUMBLE_RETURN_FAILURE;
                        } else if ((!spam and sa_modifyifham) or(spam and sa_modifyifspam)) {
                            fp = fopen(filename, "wb");
                            if (!fp) {
                                perror("Couldn't open file!");
                                return (RUMBLE_RETURN_OKAY);
                            }

                            while ((line = rcread(&s))) {
                                if (fwrite(line, strlen(line), 1, fp) != 1) break;
                            }

                            printf("[SA]: Modified %s\n", filename);
                            fclose(fp);
                        }
                    } else free(line);
                } else {
                    printf("[SA]: Spamd hung up :(\n");
                }
            }

            if (c.socket) disconnect(c.socket);
        } else fclose(fp);
    } else {

        /*~~~~~~~~~~~~~~~~~~~~~~~*/
        int     spam = 0,
                x = 0;
        char    tempfile[L_tmpnam];
        /*~~~~~~~~~~~~~~~~~~~~~~~*/

        printf("[SA]: Running check\n");
        fclose(fp);
        memset(tempfile, 0, L_tmpnam);
        tmpnam(tempfile);
#ifdef RUMBLE_MSC
        sprintf(buffer, "< \"%s\" > \"%s\"", filename, tempfile);
        printf("Executing: %s\n", buffer);
        x = execl(sa_exec, buffer, 0);
        printf("[SA]: execl returned %i\n", x);

        /*
         * ShellExecuteA( NULL, "open", sa_exec,buffer, "",SW_SHOW);
         */
#else
        sprintf(buffer, "%s < %s > %s", sa_exec, filename, tempfile);
        printf("Executing: %s\n", buffer);
        system(buffer);
#endif
        fp = fopen(tempfile, "rb");
        if (fp) {
            if (!fgets(buffer, 2000, fp)) memset(buffer, 0, 2000);
            while (strlen(buffer) > 2) {
                printf("%s\n", buffer);
                if (strstr(buffer, "X-Spam-Status: Yes")) {
                    spam = 1;
                }

                if (strstr(buffer, "X-Spam-Status: No")) {
                    spam = 0;
                }

                if (!fgets(buffer, 2000, fp)) break;
            }

            fclose(fp);
        }

        printf("[SA]: The message is %s!\n", spam ? "SPAM" : "not spam");
        if (spam and sa_deleteifspam) {
            printf("[SA] Deleting %s\n", filename);
            unlink(filename);
            unlink(tempfile);
            ret = RUMBLE_RETURN_FAILURE;
        } else if ((!spam and sa_modifyifham) or(spam and sa_modifyifspam)) {
            unlink(filename);
            printf("Moving modified file\n");
            if (rename(tempfile, filename)) {
                printf("[SA] Couldn't move file :(\n");
            }

            unlink(tempfile);
        }
    }

    return (ret);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {
    modinfo->title = "SpamAssassin plugin";
    modinfo->description = "Enables support for SpamAssassin mail filtering.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";
    sa_config = rumble_readconfig("spamassassin.conf");
    sa_usedaemon = atoi(rumble_get_dictionary_value(sa_config, "usespamd"));
    sa_spamscore = atoi(rumble_get_dictionary_value(sa_config, "spamscore"));
    sa_modifyifspam = atoi(rumble_get_dictionary_value(sa_config, "modifyifspam"));
    sa_modifyifham = atoi(rumble_get_dictionary_value(sa_config, "modifyifham"));
    sa_deleteifspam = atoi(rumble_get_dictionary_value(sa_config, "deleteifspam"));
    sa_port = atoi(rumble_get_dictionary_value(sa_config, "spamdport"));
    sa_enabled = atoi(rumble_get_dictionary_value(sa_config, "enablespamassassin"));
    strcpy(sa_host, rumble_get_dictionary_value(sa_config, "spamdhost"));
    strcpy(sa_exec, rumble_get_dictionary_value(sa_config, "spamexecutable"));
    myMaster = (masterHandle *) master;
    if (sa_enabled) {
        rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_CUE_SMTP_DATA + RUMBLE_HOOK_AFTER, sa_check);
    } else rumble_debug(master, "SpamAssassin", "This module is currently disabled via spamassassin.conf!\r\n");
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
        return (myConfig);
    }

    printf("%s = %s\n", key, value);
    if (!strcmp(key, "Enabled") && value) sa_enabled = atoi(value);
    if (!strcmp(key, "SpamScore") && value) sa_spamscore = atoi(value);
    if (!strcmp(key, "ModifyIfSpam") && value) sa_modifyifspam = atoi(value);
    if (!strcmp(key, "ModifyIfHam") && value) sa_modifyifham = atoi(value);
    if (!strcmp(key, "DeleteIfSpam") && value) sa_deleteifspam = atoi(value);
    if (!strcmp(key, "UseDaemon") && value) sa_usedaemon = atoi(value);
    if (!strcmp(key, "PortNumber") && value) sa_port = atoi(value);
    if (!strcmp(key, "HostName") && value) strcpy(sa_host, value);
    if (!strcmp(key, "SpamExecutable") && value) strcpy(sa_exec, value);
    printf("sa_exec = %s\n", sa_exec);
    cfgpath = rumble_config_str(myMaster, "config-dir");
    sprintf(filename, "%s/spamassassin.conf", cfgpath);
    cfgfile = fopen(filename, "w");
    if (cfgfile) {
        fprintf(cfgfile,
                "\
# Set the value below to 1 to enable the SpamAssassin module or 0 to disable it (default is 0).\n\
EnableSpamAssassin %u\n\
\n\
# At what score do we consider the message spam? (default is 5)\n\
SpamScore          %u\n\
\n\
# If the message is flagged as spam, should we modify it (include spamassassin headers)?\n\
ModifyIfSpam       %u\n\
\n\
# OR shoulw we just delete the file? (1=yes, 0=no)\n\
DeleteIfSpam       %u\n\
\n\
# If the message is ham (not spam), should we still modify the message? (1=yes, 0=no)\n\
ModifyIfHam        %u\n\
\n\
# Use the SpamAssassin daemon (fast) or a local executable (slower)? (1=yes, 0=no)\n\
UseSpamD           %u\n\
\n\
# SpamD settings\n\
<if compare(UseSpamD = 1)>\n\
	Comment            Using SpamD server\n\
	SpamDHost          %s\n\
	SpamDPort          %u\n\
	\n\
# Spamassassin executable settings\n\
<else>\n\
    Comment            Using spamassassin executable.\n\
		SpamExecutable    %s \n\
	</if>\n\
</if>\n",
            sa_enabled, sa_spamscore, sa_modifyifspam, sa_modifyifham, sa_deleteifspam, sa_usedaemon, sa_host, sa_port, sa_exec);
        fclose(cfgfile);
    }

    return (0);
}
