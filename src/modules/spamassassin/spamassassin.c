/* 
 * File:   spamassassin.c
 * Author: Humbedooh
 *
 * Created on 13. june 2011, 20:11
 */

#include "../../rumble.h"
#include "../../comm.h"

/* include <Ws2tcpip.h> */
dvector* sa_config;
int sa_spamscore, sa_modifyifspam, sa_modifyifham, sa_deleteifspam, sa_port, sa_usedaemon, sa_enabled;
const char *sa_host, *sa_exec;

static int sa_compare_value(const char *key, const char *oper, const char *value) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char* lkey;
	const char *cval;
    lkey = strclone(key);
    rumble_string_lower(lkey);
    cval = rrdict(sa_config, lkey);
    
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    free(lkey);
    if (!cval) return (0);
    
    if (!strcmp(oper, "=")) return (!strcmp(value, cval));
    if (!strcmp(oper, ">")) return (atoi(value) < atoi(cval));
    if (!strcmp(oper, "<")) return (atoi(value) > atoi(cval));
    if (!strcmp(oper, ">=")) return (atoi(value) <= atoi(cval));
    if (!strcmp(oper, "<=")) return (atoi(value) >= atoi(cval));
    if (!strcmp(oper, "!=")) return (atoi(value) != atoi(cval));
    return (0);
}


/*
 =======================================================================================================================
 =======================================================================================================================
 */
void sa_config_load(masterHandle *master) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    char                cfgfile[4096];
    FILE                *config;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sa_config = dvector_init();
    sprintf(cfgfile, "%s/spamassassin.conf", ((masterHandle *) master)->cfgdir);
    config = fopen(cfgfile, "r");
    if (config) {

        /*~~~~~~~~~~~~~~~~~~~~~~~*/
        int             p = 0;
        unsigned int    ignore = 0,
                        n = 0;
        char            *key,
                        *value,
                        *buffer,
                        line[512],
                        ckey[129],
                        coper[3],
                        cval[129];
        /*~~~~~~~~~~~~~~~~~~~~~~~*/

        memset(ckey, 0, 129);
        memset(cval, 0, 129);
        memset(coper, 0, 3);
        buffer = (char *) malloc(512);
        key = (char *) calloc(1, 512);
        value = (char *) calloc(1, 512);
        if (!buffer || !key || !value) merror();
        while (!feof(config)) {
            memset(buffer, 0, 512);
            memset(line, 0, 512);
            if (!fgets(buffer, 511, config)) break;
            if (sscanf(buffer, "%*[ \t]%511[^\r\n]", line) == 0) sscanf(buffer, "%511[^\r\n]", line);
            p++;
            if (!ferror(config)) {
                memset(key, 0, 512);
                memset(value, 0, 512);
                if (sscanf(line, "<%511[^ \t>]%*[ \t]%511[^\r\n]>", key, value) >= 1) {
                    rumble_string_lower(key);
                    rumble_string_lower(value);
                    if (!strcmp(key, "/if")) ignore >>= 1;
                    if (!strcmp(key, "if")) {
                        ignore = (ignore << 1) + 1;
                        if (sscanf(value, "compare(%128[^) \t]%*[ \t]%128[^) \t]%*[ \t]%128[^) \t]%*[ \t])", ckey, coper, cval) == 3)
                            if (sa_compare_value(ckey, coper, cval)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "defined(%128[^) \t])", ckey) == 1)
                            if (rhdict(sa_config, ckey)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "exists(%128[^) \t])", ckey) == 1)
                            if (rumble_file_exists(ckey)) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else-if") && !(ignore & 0xFFFFFFFE)) {
                        ignore &= 0xFFFFFFFE;
                        ignore++;
                        if (sscanf(value, "compare(%128[^) \t]%*[ \t]%128[^) \t]%*[ \t]%128[^) \t]%*[ \t])", ckey, coper, cval) == 3)
                            if (sa_compare_value(ckey, coper, cval)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "defined(%128[^) \t])", ckey) == 1)
                            if (rhdict(sa_config, ckey)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "exists(%128[^) \t])", ckey) == 1)
                            if (rumble_file_exists(ckey)) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else") && !(ignore & 0xFFFFFFFE)) ignore = (ignore & 0xFFFFFFFE) | (!(ignore & 0x00000001));
                }

                if (sscanf(line, "%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    if (!strcmp(key, "comment")) {
                        printf("%s\r\n", value);
                        statusLog("CFG: %s", value);
                    } else rsdict(sa_config, key, value);
                } else if (sscanf(line, "%*[ \t]%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    rsdict(sa_config, key, value);
                }
            } else {
                statusLog("ERROR: Could not read %s!", cfgfile);
                fprintf(stderr, "<config> Error: Could not read %s!\n", cfgfile);
                exit(EXIT_FAILURE);
            }
        }

        free(buffer);
        fclose(config);
    } else {
        statusLog("ERROR: Could not open %s!", cfgfile);
        fprintf(stderr, "<config> Error: Could not read %s!\n", cfgfile);
        exit(EXIT_FAILURE);
    }
}

ssize_t sa_check(sessionHandle *session, const char *filename) {
	char buffer[2001], *line, b;
	FILE* fp;
	size_t fsize,bread;
	ssize_t ret = RUMBLE_RETURN_OKAY;

	printf("[SA]: SpamAssassin plugin is now checking %s...\n", filename ? filename : "??");

	fp = fopen(filename, "rb");
    if (!fp) {
        perror("Couldn't open file!");
        return (RUMBLE_RETURN_OKAY);
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    rewind(fp);

    // Use the spamd server?
    if (sa_usedaemon) {
        sessionHandle               s;
        clientHandle                c;
        
        s.client = &c;
        s._svc = 0;
        printf("[SA]: Connecting to spamd <%s>...\n", sa_host);
        c.socket = comm_open((masterHandle*) session->_master, sa_host, sa_port);
        c.tls = 0;
        c.recv = 0;
        c.send = 0;
        FD_ZERO(&c.fd);
        FD_SET(c.socket, &c.fd);
        
        if (c.socket) {
			printf("Connected to spamd, sending request\n");
			if (sa_modifyifham || (sa_modifyifspam and !sa_deleteifspam)) rcsend(&s, "PROCESS SPAMC/1.5\r\n");
			else rcsend(&s, "CHECK SPAMC/1.5\r\n");
			rcprintf(&s, "Content-length: %u\r\n\r\n", fsize);
			while (!feof(fp) && fp) {
				memset(buffer, 0, 2000);
				bread = fread(buffer, 1, 2000, fp);
				send(c.socket, buffer, (int) bread, 0);
			}
			fclose(fp);
			if (c.socket) {
				int x = 0, spam = 0;
				printf("[SA] Recieving response...\n");
				line = rcread(&s);
				if (line) {
					printf("%s\n", line);
					if (strstr(line, "EX_OK")) {
						while (strlen(line) > 2) {
							free(line);
							line = rcread(&s);
							if (!line) break;
							printf("%s\n", line);
							if (strstr(line, "Spam: True")) { spam = 1; }
							if (strstr(line, "Spam: False")) { spam = 0; }
						}
						free(line);
						printf("[SA]: The message is %s!\n", spam ? "SPAM" : "not spam");
						if (spam and sa_deleteifspam) {
							printf("[SA] Deleting %s\n", filename);
							unlink(filename);
							ret = RUMBLE_RETURN_FAILURE;
						}
						else if ( (!spam and sa_modifyifham) or (spam and sa_modifyifspam) ) {
							fp = fopen(filename, "wb");
							if (!fp) {
								perror("Couldn't open file!");
								return (RUMBLE_RETURN_OKAY);
							}
							while ( (line = rcread(&s)) ) {
								if (fwrite(line, strlen(line), 1, fp) != 1) break;
							}
							printf("[SA]: Modified %s\n", filename);
							fclose(fp);
						}
					}
					else free(line);
				}
				else {
					printf("[SA]: Spamd hung up :(\n");
				}
				
			}
			if (c.socket) disconnect(c.socket);
        }
		else fclose(fp);
    }
    return ret;
}
/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {
    modinfo->title = "SpamAssassin plugin";
    modinfo->description = "Enables support for SpamAssassin mail filtering";
    modinfo->author = "Humbedooh (humbedooh@users.sf.net)";
    sa_config_load((masterHandle*) master);
    sa_usedaemon = atoi(rumble_get_dictionary_value(sa_config, "usespamd"));
    sa_spamscore = atoi(rumble_get_dictionary_value(sa_config, "spamscore"));
    sa_modifyifspam = atoi(rumble_get_dictionary_value(sa_config, "modifyifspam"));
    sa_modifyifham = atoi(rumble_get_dictionary_value(sa_config, "modifyifham"));
    sa_deleteifspam = atoi(rumble_get_dictionary_value(sa_config, "deleteifspam"));
    sa_port = atoi(rumble_get_dictionary_value(sa_config, "spamdport"));
	sa_enabled = atoi(rumble_get_dictionary_value(sa_config, "enablespamassassin"));
    sa_host = rumble_get_dictionary_value(sa_config,"spamdhost");
    sa_exec = rumble_get_dictionary_value(sa_config,"spamexecutable");
	
	

    if (sa_enabled) rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_CUE_SMTP_DATA + RUMBLE_HOOK_AFTER, sa_check);
	else printf("[SpamAssassin]: This module is currently disabled via spamassassin.conf!\r\n");
    
    
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
