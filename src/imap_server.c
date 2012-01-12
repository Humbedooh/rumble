/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "servers.h"
#include "comm.h"
#include "private.h"
#include "database.h"
#include "mailman.h"

/*
 =======================================================================================================================
    Main loop
 =======================================================================================================================
 */
void *rumble_imap_init(void *T) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleThread    *thread = (rumbleThread *) T;
    rumbleService   *svc = thread->svc;
    masterHandle    *master = svc->master;
    /* Initialize a session handle and wait for incoming connections. */
    sessionHandle   session;
    sessionHandle   *sessptr = &session;
    ssize_t         rc;
    char            *extra_data,
                    *cmd,
                    *parameters,
                    *line;
    const char      *myName;
    sessionHandle   *s;
    accountSession  *pops;
    d_iterator      iter;
    svcCommandHook  *hook;
    c_iterator      citer;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    session.dict = dvector_init();
    session.recipients = dvector_init();
    session._svcHandle = (accountSession *) malloc(sizeof(accountSession));
    session._svc = svc;
    session.client = (clientHandle *) malloc(sizeof(clientHandle));
    session.client->tls = 0;
    session.client->send = 0;
    session.client->recv = 0;
    session._master = svc->master;
    pops = (accountSession *) session._svcHandle;
    pops->account = 0;
    pops->bag = 0;
    pops->folder = 0;
    session._tflags = RUMBLE_THREAD_IMAP;   /* Identify the thread/session as IMAP4 */
    myName = rrdict(master->_core.conf, "servername");
    myName = myName ? myName : "??";
    while (1) {
        comm_accept(svc->socket, session.client);
        pthread_mutex_lock(&svc->mutex);
        dvector_add(svc->handles, (void *) sessptr);
        svc->traffic.sessions++;
        pthread_mutex_unlock(&svc->mutex);
        session.flags = 0;
        session._tflags += 0x00100000;      /* job count ( 0 through 4095) */
        session.sender = 0;
        pops->bag = 0;
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        rumble_debug(NULL, "imap4", "Accepted connection from %s on IMAP4", session.client->addr);
#endif

        /* Check for hooks on accept() */
        rc = RUMBLE_RETURN_OKAY;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_IMAP);
        if (rc == RUMBLE_RETURN_OKAY) rcprintf(sessptr, "* OK <%s> IMAP4rev1 Service Ready\r\n", myName);   /* Hello! */
        else svc->traffic.rejections++;

        /* Parse incoming commands */
        extra_data = (char *) malloc(32);
        cmd = (char *) malloc(32);
        parameters = (char *) malloc(1024);
        if (!cmd || !parameters || !extra_data) merror();
        while (rc != RUMBLE_RETURN_FAILURE) {
            memset(extra_data, 0, 32);
            memset(cmd, 0, 32);
            memset(parameters, 0, 1024);
            line = rumble_comm_read(sessptr);
            rc = 421;
            if (!line) break;
            rc = 105;   /* default return code is "500 unknown command thing" */
            if (sscanf(line, "%32s %32s %1000[^\r\n]", extra_data, cmd, parameters)) {
                rumble_string_upper(cmd);

                /*
                 * rumble_debug(NULL, "imap4", "Client <%p> said: %s %s", &session, cmd, parameters);
                 */
                if (!strcmp(cmd, "UID")) {

                    /* Set UID flag if requested */
                    session.flags |= rumble_mailman_HAS_UID;
                    if (sscanf(parameters, "%32s %1000[^\r\n]", cmd, parameters)) rumble_string_upper(cmd);
                } else session.flags -= (session.flags & rumble_mailman_HAS_UID);   /* clear UID demand if not there. */
                cforeach((svcCommandHook *), hook, svc->commands, citer) {
                    if (!strcmp(cmd, hook->cmd)) rc = hook->func(master, &session, parameters, extra_data);
                }

                /*
                 * rumble_debug(NULL, "imap4", "%s said: <%s> %s %s", session.client->addr, extra_data, cmd, parameters);
                 * ;
                 * printf("Selected folder is: %"PRId64 "\r\n", pops->folder);
                 */
            }

            free(line);
            if (rc == RUMBLE_RETURN_IGNORE) {

                /*
                 * printf("Ignored command: %s %s\n",cmd, parameters);
                 */
                continue;   /* Skip to next line. */
            } else if (rc == RUMBLE_RETURN_FAILURE) {
                svc->traffic.rejections++;
                break;      /* Abort! */
            } else rcprintf(&session, "%s BAD Invalid command!\r\n", extra_data);   /* Bad command thing. */
        }

        /* Cleanup */
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        rumble_debug(NULL, "imap4", "Closing connection to %s on IMAP4", session.client->addr);
#endif
        if (rc == 421) {

            /*
             * rcprintf(&session, "%s BAD Session timed out!\r\n", extra_data);
             * timeou
             */
        } else {
            rcsend(&session, "* BYE bye!\r\n");
            rcprintf(&session, "%s OK <%s> signing off for now.\r\n", extra_data, myName);
        }

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Run hooks scheduled for service closing
         ---------------------------------------------------------------------------------------------------------------
         */

        /*
         * rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_CLOSE + RUMBLE_HOOK_IMAP);
         */
        comm_addEntry(svc, session.client->brecv + session.client->bsent, session.client->rejected);
        disconnect(session.client->socket);
        printf("Cleaning up\n");

        /* Start cleanup */
        free(parameters);
        free(cmd);
        rumble_clean_session(sessptr);
        rumble_free_account(pops->account);
        mailman_close_bag(pops->bag);
        pops->bag = 0;

        /* End cleanup */
        pthread_mutex_lock(&(svc->mutex));
        foreach((sessionHandle *), s, svc->handles, iter) {
            if (s == sessptr) {
                dvector_delete(&iter);
                break;
            }
        }

        /* Check if we were told to go kill ourself :( */
        if ((session._tflags & RUMBLE_THREAD_DIE) || svc->enabled != 1 || thread->status == -1) {

            /*~~~~~~~~~~~~~~~*/
            rumbleThread    *t;
            /*~~~~~~~~~~~~~~~*/

#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
            printf("<imap4::threads>I (%#x) was told to die :(\n", (uintptr_t) pthread_self());
#endif
            cforeach((rumbleThread *), t, svc->threads, citer) {
                if (t == thread) {
                    cvector_delete(&citer);
                    break;
                }
            }

            pthread_mutex_unlock(&svc->mutex);
            pthread_exit(0);
        }

        pthread_mutex_unlock(&svc->mutex);
        myName = rrdict(master->_core.conf, "servername");
        myName = myName ? myName : "??";
    }

    pthread_exit(0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_imap_login(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            user[256],
                    pass[256],
                    digest[1024];
    address         *addr;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mailman_close_bag(imap->bag);
    if (sscanf(parameters, "\"%256[^\" ]\" \"%256[^\" ]\"", user, pass) == 2 or sscanf(parameters, "%255s %255s", user, pass) == 2) {
        sprintf(digest, "<%s>", user);
        addr = rumble_parse_mail_address(digest);
        if (addr) {
            rumble_debug(NULL, "imap4", "%s requested access to %s@%s via LOGIN\n", session->client->addr, addr->user, addr->domain);
            imap->account = rumble_account_data_auth(0, addr->user, addr->domain, pass);
            if (imap->account) {
                rumble_debug(NULL, "imap4", "%s's request for %s@%s was granted\n", session->client->addr, addr->user, addr->domain);
                rcprintf(session, "%s OK Welcome!\r\n", extra_data);
                imap->folder = 0;
                imap->bag = mailman_get_bag(imap->account->uid,
                                            strlen(imap->account->domain->path) ? imap->account->domain->path : rrdict(master->_core.conf, "storagefolder"));
            } else {
                rumble_debug(NULL, "imap4", "%s's request for %s@%s was denied (wrong pass?)\n", session->client->addr, addr->user,
                             addr->domain);
                rcprintf(session, "%s NO Incorrect username or password!\r\n", extra_data);
                session->client->rejected = 1;
            }
        } else {
            rcprintf(session, "%s NO Incorrect username or password!\r\n", extra_data);
            session->client->rejected = 1;
        }
    } else {
        rcprintf(session, "%s NO Incorrect username or password!\r\n", extra_data);
        session->client->rejected = 1;
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    NOOP
 =======================================================================================================================
 */
ssize_t rumble_server_imap_noop(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {
    rcprintf(session, "%s OK Doin' nothin'...\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CAPABILITY
 =======================================================================================================================
 */
ssize_t rumble_server_imap_capability(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~*/
    char        capa[1024];
    char        *el;
    c_iterator  iter;
    /*~~~~~~~~~~~~~~~~~~~*/

    sprintf(capa, "* CAPABILITY");
    cforeach((char *), el, ((rumbleService *) session->_svc)->capabilities, iter) {
        sprintf(&capa[strlen(capa)], " %s", el);
    }

    sprintf(&capa[strlen(capa)], "\r\n");
    rcsend(session, capa);
    rcprintf(session, "%s OK CAPABILITY completed.\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    AUTHENTICATE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_authenticate(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    accountSession  *imap = (accountSession *) session->_svcHandle;
    char            method[32],
                    tmp[258],
                    user[256],
                    pass[256],
                    *line,
                    *buffer;
    address         *addr = 0;
    int             x = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mailman_close_bag(imap->bag);
    imap->bag = 0;
    if (sscanf(strchr(parameters, '"') ? strchr(parameters, '"') + 1 : parameters, "%32[a-zA-Z]", method)) {
        rumble_string_upper(method);
        if (!strcmp(method, "PLAIN")) {
            rcprintf(session, "%s OK Method <%s> accepted, input stuffs!\r\n", extra_data, method);
            line = rcread(session);
            if (line) {
                buffer = rumble_decode_base64(line);
                if (sscanf(buffer + 1, "\"%255[^\"]\"", user)) x = 2;
                else sscanf(buffer + 1, "%255s", user);
                if (!sscanf(buffer + 2 + x + strlen(user), "\"%255[^\"]\"", pass)) sscanf(buffer + 2 + x + strlen(user), "%255s", pass);
                sprintf(tmp, "<%s>", user);
                if (pass[strlen(pass) - 1] == 4) pass[strlen(pass) - 1] = 0;    /* remove EOT character if present. */
                addr = rumble_parse_mail_address(tmp);
                if (addr) {
                    rumble_debug(NULL, "imap4", "%s requested access to %s@%s via AUTHENTICATE", session->client->addr, addr->user,
                                 addr->domain);
                    imap->account = rumble_account_data_auth(0, addr->user, addr->domain, pass);
                    if (imap->account) {
                        rcprintf(session, "%s OK Welcome!\r\n", extra_data);
                        imap->folder = 0;

                        /* Check if we have a shared mailbox instance available, if not, make one */
                        imap->bag = mailman_get_bag(imap->account->uid,
                                                    strlen(imap->account->domain->path) ? imap->account->domain->path : rrdict(
                                                        master->_core.conf, "storagefolder"));
                    } else {
                        rcprintf(session, "%s NO Incorrect username or password!\r\n", extra_data);
                        session->client->rejected = 1;
                    }

                    rumble_free_address(addr);
                } else {
                    rcprintf(session, "%s NO Incorrect username or password!\r\n", extra_data);
                    session->client->rejected = 1;
                }

                free(buffer);
                free(line);
            }
        }
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    SELECT
 =======================================================================================================================
 */
ssize_t rumble_server_imap_select(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        exists,
                    recent,
                    first;
    rumble_args     *params;
    mailman_folder  *folder;
    char            *selector;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* Are we authed? */
    if (imap->account) {
        params = rumble_read_words(parameters);
        selector = params->argc ? params->argv[0] : "";

        /* Get the folder */
        folder = mailman_get_folder(imap->bag, selector);
        if (folder) {

            /*~~~~~~~~~~~~~~~~~~~~*/
            mailman_letter  *letter;
            uint32_t        i;
            /*~~~~~~~~~~~~~~~~~~~~*/

            mailman_update_folder(folder, imap->bag->uid, 0);
            imap->folder = folder;
            rumble_rw_start_read(imap->bag->lock);
            session->flags |= rumble_mailman_HAS_SELECT;
            session->flags |= rumble_mailman_HAS_READWRITE;
            exists = 0;
            recent = 0;
            first = 0;

            /* Retrieve the statistics of the folder */
            for (i = 0; i < folder->size; i++) {
                letter = &folder->letters[i];
                if (!letter->inuse) continue;
                exists++;
                if (!first && (letter->flags & RUMBLE_LETTER_RECENT)) first = exists;
                if (letter->flags & RUMBLE_LETTER_RECENT) {
                    letter->flags -= RUMBLE_LETTER_RECENT;
                    letter->updated = 1;
                    recent++;
                }
            }

            rumble_rw_stop_read(imap->bag->lock);
            printf("* %u EXISTS\r\n", exists);
            rcprintf(session, "* %u EXISTS\r\n", exists);
            rcsend(session, "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n");
            if (recent) {
                printf("* %u RECENT\r\n", recent);
                rcprintf(session, "* %u RECENT\r\n", recent);
            }

            if (first) {
                rcprintf(session, "* OK [UNSEEN %"PRIu64 "] Message %"PRIu64 " is the first unseen message.\r\n", first, first);
            }

            rcprintf(session, "* OK [UIDVALIDITY %08u] UIDs valid\r\n", imap->account->uid);
            rcprintf(session, "%s OK [READ-WRITE] SELECT completed.\r\n", extra_data);
        } else {
            rcprintf(session, "%s NO No such mailbox <%s>!\r\n", extra_data, selector);
        }

        /* Shared Object Reader Unlock */
        rumble_args_free(params);
    } else {
        rcprintf(session, "%s NO Not logged in yet!\r\n", extra_data);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    EXAMINE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_examine(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        exists,
                    recent,
                    first;
    rumble_args     *params;
    mailman_folder  *folder;
    char            *selector;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    mailman_letter  *letter;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* Are we authed? */
    if (imap->account) {
        params = rumble_read_words(parameters);
        selector = params->argc ? params->argv[0] : "";
        folder = mailman_get_folder(imap->bag, selector);
        if (folder) {
            rumble_rw_start_read(imap->bag->lock);
            session->flags |= rumble_mailman_HAS_SELECT;
            session->flags |= rumble_mailman_HAS_READWRITE;
            exists = 0;
            recent = 0;
            first = 0;
            for (i = 0; i < folder->size; i++) {
                letter = &folder->letters[i];
                if (!letter->inuse) continue;
                exists++;
                if (!first && (letter->flags & RUMBLE_LETTER_RECENT)) first = exists;
            }

            rumble_rw_stop_read(imap->bag->lock);
            rcprintf(session, "* %u EXISTS\r\n", exists);
            rcsend(session, "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n");
            if (recent) rcprintf(session, "* %u RECENT.\r\n", recent);
            if (first) rcprintf(session, "* OK [UNSEEN %u] Message %u is the first unseen message.\r\n", first, first);
            rcprintf(session, "* OK [UIDVALIDITY %08u] UIDs valid\r\n", imap->account->uid);
            rcprintf(session, "%s OK [READ-ONLY] EXAMINE completed.\r\n", extra_data);
        } else {
            rcprintf(session, "%s NO No such mailbox <%s>!\r\n", extra_data, selector);
        }
    } else {
        rcprintf(session, "%s NO Not logged in yet!\r\n", extra_data);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CREATE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_create(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *args;
    char            *newName;
    mailman_folder  *newFolder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);
    newFolder = 0;
    if (args && args->argc == 1) {
        newName = args->argv[0];

        /* Shared Object Writer Lock */
        newFolder = mailman_get_folder(imap->bag, newName);
        if (newFolder) rcprintf(session, "%s NO CREATE failed: Duplicate folder name.\r\n", extra_data);
        else {
            rumble_rw_start_write(imap->bag->lock);

            /* Add the folder to the SQL DB */
            radb_run_inject(master->_core.db, "INSERT INTO folders (uid, name) VALUES (%u, %s)", imap->account->uid, newName);

            /* Update the local folder list */
            rumble_rw_stop_write(imap->bag->lock);
            mailman_update_folders(imap->bag);
            rcprintf(session, "%s OK CREATE completed\r\n", extra_data);
        }

        /* Shared Object Writer Unlock */
    } else rcprintf(session, "%s BAD Invalid CREATE syntax!\r\n", extra_data);
    if (args) rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    DELETE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_delete(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    accountSession  *imap = (accountSession *) session->_svcHandle;
    rumble_args     *args;
    mailman_folder  *folder = 0;
    char            *folderName = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* Are we authed? */
    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);

    /* Find the folder we're looking for */
    if (args && args->argc >= 1) {
        folderName = args->argv[0];
        folder = mailman_get_folder(imap->bag, folderName);
    }

    if (!folder) {
        rcprintf(session, "%s NO DELETE failed: No such folder <%s>\r\n", extra_data, folderName);
        return (RUMBLE_RETURN_IGNORE);
    }

    /* Obtain write lock on the bag */
    rumble_rw_start_write(imap->bag->lock);

    /* Delete folder from database and bag struct */
    radb_run_inject(master->_core.db, "DELETE FROM folders WHERE id = %u", folder->fid);
    mailman_delete_folder(imap->bag, folder);
    rumble_rw_stop_write(imap->bag->lock);
    rcprintf(session, "%s OK Deleted <%s>\r\n", extra_data, folderName);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    RENAME
 =======================================================================================================================
 */
ssize_t rumble_server_imap_rename(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *args;
    char            *oldName,
                    *newName;
    mailman_folder  *oldFolder,
                    *newFolder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);
    oldFolder = 0;
    newFolder = 0;
    if (args && args->argc == 2) {
        oldName = args->argv[0];
        newName = args->argv[1];

        /* Shared Object Writer Lock */
        oldFolder = mailman_get_folder(imap->bag, oldName);
        newFolder = mailman_get_folder(imap->bag, newName);
        if (newFolder) rcprintf(session, "%s NO RENAME failed: Duplicate folder name.\r\n", extra_data);
        else if (!oldFolder)
            rcprintf(session, "%s NO RENAME failed: No such folder <%s>\r\n", extra_data, oldName);
        else {
            rumble_rw_start_write(imap->bag->lock);
            radb_run_inject(master->_core.db, "UPDATE folders set name = %s WHERE id = %u", newName, oldFolder->fid);
            strncpy(oldFolder->name, newName, 64);
            rcprintf(session, "%s OK RENAME completed\r\n", extra_data);
            rumble_rw_stop_write(imap->bag->lock);
        }
    } else rcprintf(session, "%s BAD Invalid RENAME syntax!\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    SUBSCRIBE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_subscribe(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *args;
    char            *folderName;
    mailman_folder  *folder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);
    folder = 0;
    if (args && args->argc == 1) {
        folderName = args->argv[0];

        /* Shared Object Writer Lock */
        folder = mailman_get_folder(imap->bag, folderName);
        if (!folder) rcprintf(session, "%s NO SUBSCRIBE failed: No such folder <%s>\r\n", extra_data, folderName);
        else {
            rumble_rw_start_write(imap->bag->lock);
            radb_run_inject(master->_core.db, "UPDATE folders set subscribed = 1 WHERE id = %l", folder->fid);
            folder->subscribed = 1;
            rumble_rw_stop_write(imap->bag->lock);
            rcprintf(session, "%s OK SUBSCRIBE completed\r\n", extra_data);
        }

        /* Shared Object Writer Unlock */
    } else rcprintf(session, "%s BAD Invalid SUBSCRIBE syntax!\r\n", extra_data);
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    UNSUBSCRIBE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_unsubscribe(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *args;
    char            *folderName;
    mailman_folder  *folder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);
    folder = 0;
    if (args && args->argc == 1) {
        folderName = args->argv[0];

        /* Shared Object Writer Lock */
        folder = mailman_get_folder(imap->bag, folderName);
        if (!folder) rcprintf(session, "%s NO UNSUBSCRIBE failed: No such folder <%s>\r\n", extra_data, folderName);
        else {
            rumble_rw_start_write(imap->bag->lock);
            radb_run_inject(master->_core.db, "UPDATE folders set subscribed = 0 WHERE id = %l", folder->fid);
            folder->subscribed = 0;
            rumble_rw_stop_write(imap->bag->lock);
            rcprintf(session, "%s OK UNSUBSCRIBE completed\r\n", extra_data);
        }

        /* Shared Object Writer Unlock */
    } else rcprintf(session, "%s BAD Invalid UNSUBSCRIBE syntax!\r\n", extra_data);
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    LIST
 =======================================================================================================================
 */
ssize_t rumble_server_imap_list(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *args;
    char            *pattern,
                    *xpattern;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    int             x;
    uint32_t        i;
    mailman_folder  *folder;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);
    if (args && args->argc == 2) {
        pattern = args->argv[1];
        rumble_rw_start_read(imap->bag->lock);
        folder = imap->folder;
        if (!folder) rcsend(session, "* LIST (\\Noselect) \".\" \"\"\r\n");
        else rcprintf(session, "* LIST (\\Noselect) \"\" \"%s\"\r\n", folder->name);
        for (i = 0; i < imap->bag->size; i++) {
            folder = &imap->bag->folders[i];
            if (folder->inuse) {
                x = strncmp(pattern, folder->name, strlen(pattern));
                xpattern = strchr(pattern, '*');
                if (x && xpattern) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    size_t  len = strlen(pattern) - strlen(xpattern);
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    if (x) x = strncmp(pattern, folder->name, len);
                }

                if (!x) {
                    rcprintf(session, "* LIST () \".\" \"%s\"\r\n", folder->name);
                    printf("* LIST () \".\" \"%s\"\r\n", folder->name);
                }
            }
        }

        rumble_rw_stop_read(imap->bag->lock);
        rcprintf(session, "%s OK LIST completed\r\n", extra_data);
    } else rcprintf(session, "%s BAD Invalid LIST syntax!\r\n", extra_data);
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    LSUB
 =======================================================================================================================
 */
ssize_t rumble_server_imap_lsub(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *args;
    char            *mbox,
                    *pattern;
    mailman_folder  *folder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);
    if (args && args->argc == 2) {
        mbox = args->argv[0];
        pattern = args->argv[1];

        /* Shared Object Reader Lock */
        rumble_rw_start_read(imap->bag->lock);
        if (imap->folder) {
            rcprintf(session, "* LSUB () \".\" \"INBOX\"\r\n");
            for (i = 0; i < imap->bag->size; i++) {
                folder = &imap->bag->folders[i];
                if (folder->inuse && folder->subscribed) rcprintf(session, "* LSUB () \".\" \"%s\"\r\n", folder->name);
            }
        }

        /* Shared Object Reader Unlock */
        rumble_rw_stop_read(imap->bag->lock);
        rcprintf(session, "%s OK LSUB completed\r\n", extra_data);
    } else rcprintf(session, "%s BAD Invalid LSUB syntax!\r\n", extra_data);
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    STATUS
 =======================================================================================================================
 */
ssize_t rumble_server_imap_status(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             messages = 0,
                    recent = 0,
                    unseen = 0;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    uint32_t        i,
                    x;
    rumble_args     *args;
    mailman_folder  *folder = 0;
    mailman_letter  *letter;
    char            *folderName = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    args = rumble_read_words(parameters);

    /* Find the folder we're looking for */
    folder = 0;
    if (args && args->argc >= 1) {
        folderName = args->argv[0];
        folder = mailman_get_folder(imap->bag, folderName);
        if (!folder) {
            rcprintf(session, "%s NO STATUS failed: No such folder <%s>\r\n", extra_data, folderName);
            return (RUMBLE_RETURN_IGNORE);
        }
    }

    mailman_update_folder(folder, imap->bag->uid, 0);

    /* Retrieve the status of the folder */
    rumble_rw_start_read(imap->bag->lock);
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (letter) {
            if ((letter->flags & RUMBLE_LETTER_UNREAD) || (letter->flags == RUMBLE_LETTER_RECENT)) unseen++;
            if (letter->flags == RUMBLE_LETTER_RECENT) recent++;
            messages++;
        }
    }

    rumble_rw_stop_read(imap->bag->lock);
    rcprintf(session, "%s STATUS %s ", extra_data, folderName);
    for (x = 1; x < args->argc; x++) {
        if (strstr(args->argv[x], "UNSEEN")) rcprintf(session, "UNSEEN %u ", unseen);
        if (strstr(args->argv[x], "RECENT")) rcprintf(session, "RECENT %u ", recent);
        if (strstr(args->argv[x], "MESSAGES")) rcprintf(session, "MESSAGES %u ", messages);
    }

    rcprintf(session, "\r\n");
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    APPEND
 =======================================================================================================================
 */
ssize_t rumble_server_imap_append(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args     *params;
    char            *destFolder;
    char            *Flags;
    uint32_t        size = 0;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    mailman_folder  *folder;
    uint32_t        readBytes = 0,
                    flags = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    params = rumble_read_words(parameters);
    if (params->argc > 1 && imap->bag) {
        printf("getting size of email\n");
        sscanf(params->argv[params->argc - 1], "{%d", &size);
        printf("size is %u bytes\n", size);
        destFolder = params->argv[0];
        Flags = params->argc > 2 ? params->argv[1] : "";

        /* Shared Object Reader Lock */
        folder = mailman_get_folder(imap->bag, destFolder);
    }

    if (strlen(Flags)) {
        if (strstr(Flags, "\\Seen")) flags |= RUMBLE_LETTER_READ;
        if (strstr(Flags, "\\Recent")) flags |= RUMBLE_LETTER_RECENT;
        if (strstr(Flags, "\\Deleted")) flags |= RUMBLE_LETTER_DELETED;
        if (strstr(Flags, "\\Flagged")) flags |= RUMBLE_LETTER_FLAGGED;
    }

    rumble_args_free(params);
    if (!size or!folder) {
        rcprintf(session, "%s BAD Invalid APPEND syntax!\r\n", extra_data);
    } else {

        /*~~~~~~~~~~~~~~*/
        char    *sf;
        char    *fid;
        char    *filename;
        FILE    *fp = 0;
        /*~~~~~~~~~~~~~~*/

        rumble_debug(NULL, "imap4", "Append required, making up new filename");
        fid = rumble_create_filename();
        sf = imap->bag->path;
        filename = (char *) calloc(1, strlen(sf) + 36);
        if (!filename) merror();
        sprintf(filename, "%s/%s.msg", sf, fid);
        rumble_debug(NULL, "imap4", "Storing new message of size %u in folder", size);
        fp = fopen(filename, "wb");
        if (fp) {

            /*~~~~~~~~~~~*/
            char    *line;
            char    OK = 1;
            /*~~~~~~~~~~~*/

            rumble_debug(NULL, "imap4", "Writing to file %s", filename);
            rcprintf(session, "%s OK Appending!\r\n", extra_data);  /* thunderbird bug?? yes it is! */
            while (readBytes < size) {
                line = rumble_comm_read_bytes(session, size > 1024 ? 1024 : size);
                if (line) {
                    readBytes += strlen(line);
                    fwrite(line, strlen(line), 1, fp);
                    free(line);
                } else {
                    OK = 0;
                    break;
                }
            }

            fclose(fp);
            if (!OK) {
                rumble_debug(NULL, "imap4", "An error occured while reading file from client");
                unlink(filename);
            } else {
                rumble_debug(NULL, "imap4", "File written OK");
                radb_run_inject(master->_core.mail, "INSERT INTO mbox (id,uid, fid, size, flags, folder) VALUES (NULL,%u, %s, %u,%u, %l)",
                                imap->account->uid, fid, size, flags, folder->fid);
                rumble_debug(NULL, "imap4", "Added message no. #%s to folder %llu of user %u", fid, folder->fid, imap->account->uid);
            }
        }

        free(filename);
        free(fid);

        /* TODO: Check if there's room for storing message */
    }

    /* 003 APPEND saved-messages (\Seen) {310} */
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CHECK
 =======================================================================================================================
 */
ssize_t rumble_server_imap_check(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CLOSE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_close(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    mailman_folder  *folder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    folder = imap->folder;
    if (folder && imap->account && (session->flags & rumble_mailman_HAS_SELECT)) {
        mailman_commit(imap->bag, folder, 0);
        session->flags -= rumble_mailman_HAS_SELECT;    /* clear select flag. */
        imap->folder = 0;
        rcprintf(session, "%s OK Expunged and closed the mailbox.\r\n", extra_data);
    } else rcprintf(session, "%s NO CLOSE: No mailbox to close!\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    EXPUNGE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_expunge(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    accountSession  *imap = (accountSession *) session->_svcHandle;
    mailman_folder  *folder = imap->folder;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (imap->account && (session->flags & rumble_mailman_HAS_SELECT) && folder) {
        mailman_commit(imap->bag, folder, 0);
        rcprintf(session, "%s OK Expunged them letters.\r\n", extra_data);
    } else rcprintf(session, "%s NO EXPUNGE: No mailbox selected for expunging!\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    SEARCH
 =======================================================================================================================
 */
ssize_t rumble_server_imap_search(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    FETCH
 =======================================================================================================================
 */
ssize_t rumble_server_imap_fetch(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    mailman_letter  *letter;
    rumble_args     *parts,
                    *params;
    mailman_folder  *folder;
    size_t          a,
                    b,
                    c,
                    d,
                    w_uid,
                    first,
                    last;
    char            line[1024];
    const char      *body,
                    *body_peek;
    int             flags,
                    uid,
                    internaldate,
                    envelope;
    int             size,
                    text,
                    x,
                    header; /* rfc822.size/text/header */
    uint32_t        octets;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    rangePair       ranges[64];
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    folder = imap->folder;
    if (!folder) {
        rcprintf(session, "%s NO No mailbox selected for fetching!\r\n", extra_data);
        return (RUMBLE_RETURN_IGNORE);
    }

    mailman_update_folder(folder, imap->account->uid, 0);
    uid = strstr(parameters, "UID") ? 1 : 0;
    internaldate = strstr(parameters, "INTERNALDATE") ? 1 : 0;
    envelope = strstr(parameters, "ENVELOPE") ? 1 : 0;
    size = strstr(parameters, "RFC822.SIZE") ? 1 : 0;
    text = strstr(parameters, "RFC822.TEXT") ? 1 : 0;
    header = strstr(parameters, "RFC822.HEADER") ? 1 : 0;
    flags = strstr(parameters, "FLAGS") ? 1 : 0;
    octets = 0;
    memset(line, 0, 1024);
    body_peek = strstr(parameters, "BODY.PEEK[");
    body = strstr(parameters, "BODY[");
    parts = 0;
    if (body) sscanf(body, "BODY[%1000[^]]<%u>", line, &octets);
    else if (body_peek)
        sscanf(body_peek, "BODY.PEEK[%1000[^]]<%u>", line, &octets);
    w_uid = session->flags & rumble_mailman_HAS_UID;
    if (body || body_peek) {
        if (strlen(line)) {

            /*~~~~~~~~~~~~~~~~~*/
            char    region[32],
                    buffer[1024];
            /*~~~~~~~~~~~~~~~~~*/

            memset(region, 0, 32);
            memset(buffer, 0, 1024);
            if (sscanf(line, "%32s (%1000c)", region, buffer) == 2) {
                parts = rumble_read_words(buffer);
                for (b = 0; b < parts->argc; b++) rumble_string_lower(parts->argv[b]);
            }
        }
    }

    params = rumble_read_words(parameters);
    rumble_scan_ranges((rangePair *) &ranges, params->argc > 0 ? params->argv[0] : "0");
    for (x = 0; ranges[x].start != 0; x++) {
        first = ranges[x].start;
        last = ranges[x].end;
        b = 0;
        a = 0;
        d = 0;
        printf("Fetching letter %lu through %lu\n", first, last);
        for (i = 0; i < folder->size; i++) {
            letter = &folder->letters[i];
            if (!letter->inuse) continue;
            a++;
            if (w_uid && (letter->id < first || (last > 0 && letter->id > last))) continue;
            if (!w_uid && (a < first || (last > 0 && a > last))) continue;
            d++;
            rcprintf(session, "* %u FETCH (", a);
            if (flags) {
                rcprintf(session, "FLAGS (%s%s%s%s) ", (letter->flags == RUMBLE_LETTER_RECENT) ? "\\Recent " : "",
                         (letter->flags & RUMBLE_LETTER_READ) ? "\\Seen " : "", (letter->flags & RUMBLE_LETTER_DELETED) ? "\\Deleted " : "",
                         (letter->flags & RUMBLE_LETTER_FLAGGED) ? "\\Flagged " : "");
            }

            if (uid || w_uid) rcprintf(session, "UID %llu ", letter->id);
            if (size) rcprintf(session, "RFC822.SIZE %u ", letter->size);
            if (internaldate) rcprintf(session, "INTERNALDATE %u ", letter->delivered);
            if (body) letter->flags -= (letter->flags & RUMBLE_LETTER_RECENT);  /* Remove \Recent flag since we're not peeking. */
            if (body || body_peek) {

                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                char    header[10240],
                        key[64];
                FILE    *fp = mailman_open_letter(imap->bag, folder, letter->id);
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                if (fp) {
                    if (parts) {
                        memset(header, 0, 10240);
                        while (fgets(line, 1024, fp)) {
                            c = strlen(line);
                            if (line[0] == '\r' || line[0] == '\n') break;
                            memset(key, 0, 64);
                            if (sscanf(line, "%63[^:]", key)) {
                                rumble_string_lower(key);
                                if (parts) {
                                    for (b = 0; b < parts->argc; b++) {
                                        if (!strcmp(key, parts->argv[b])) {
                                            if (line[c - 2] != '\r') {
                                                line[c - 1] = '\r';
                                                line[c] = '\n';
                                                line[c + 1] = 0;
                                            }

                                            strncpy(header + strlen(header), line, strlen(line));
                                        }
                                    }
                                } else {

                                    /*
                                     * if ( line[c-2] != '\r' ) {line[c-1] = '\r';
                                     * line[c] = '\n';
                                     * line[c+1] = 0;
                                     */
                                    strncpy(header + strlen(header), line, strlen(line));
                                }
                            }
                        }

                        sprintf(header + strlen(header), "\r\n \r\n");
                        rcprintf(session, "BODY[HEADER.FIELDS (%s)] {%u}\r\n", line, strlen(header));
                        rcsend(session, header);

                        /*
                         * printf("BODY[HEADER.FIELDS (%s)] {%u}\r\n", line, strlen(header));
                         * printf("%s", header);
                         */
                    } else {
                        rcprintf(session, "BODY[] {%u}\r\n", letter->size);

                        /*
                         * printf("BODY[] {%u}\r\n", letter->size);
                         */
                        memset(line, 0, 1024);
                        while (fgets(line, 1024, fp)) {
                            rcsend(session, line);

                            /*
                             * printf("%s", line);
                             */
                        }
                    }

                    fclose(fp);
                } else printf("meh, couldn't open letter file\n");
                rcsend(session, " ");
            }

            rcprintf(session, ")\r\n");
        }
    }

    if (parts) rumble_args_free(parts);
    rumble_args_free(params);
    rcprintf(session, "%s OK FETCH completed\r\n", extra_data);
    if (folder) printf("Fetched %lu letters from <%s>\n", d, folder->name);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    STORE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_store(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint64_t        first,
                    last;
    int             silent,
                    control,
                    useUID,
                    flag;
    char            args[100];
    /* Check for selected folder */
    mailman_folder  *folder;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    rumble_args     *parts;
    rangePair       ranges[64];
    int             x = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    folder = imap->folder;
    if (!folder) {
        rcprintf(session, "%s NO STORE: No mailbox selected for storing!\r\n", extra_data);
        return (RUMBLE_RETURN_IGNORE);
    }

    useUID = session->flags & rumble_mailman_HAS_UID;

    /* Get the store type */
    silent = strstr(parameters, ".SILENT") ? 1 : 0;
    control = strchr(parameters, '+') ? 1 : (strchr(parameters, '-') ? -1 : 0);
    memset(args, 0, 100);
    sscanf(parameters, "%*100[^(](%99[^)])", args);

    /* Set the master flag */
    flag = 0;
    flag |= strstr(parameters, "\\Deleted") ? RUMBLE_LETTER_DELETED : 0;
    flag |= strstr(parameters, "\\Seen") ? RUMBLE_LETTER_READ : 0;
    flag |= strstr(parameters, "\\Flagged") ? RUMBLE_LETTER_FLAGGED : 0;
    flag |= strstr(parameters, "\\Draft") ? RUMBLE_LETTER_DRAFT : 0;
    flag |= strstr(parameters, "\\Answered") ? RUMBLE_LETTER_ANSWERED : 0;

    /*
     * Process the letters ;
     * For each range, set the message stuf
     */
    parts = rumble_read_words(parameters);
    if (parts->argc > 1) {
        rumble_scan_ranges((rangePair *) &ranges, parts->argv[0]);
        for (x = 0; ranges[x].start != 0; x++) {
            first = ranges[x].start;
            last = ranges[x].end;
            printf("Storing flags for letter %lu through %lu\n", first, last);
            if (control == -1) mailman_remove_flags(folder, flag, useUID, first, last);
            if (control == 0) mailman_set_flags(folder, flag, useUID, first, last);
            if (control == 1) mailman_add_flags(folder, flag, useUID, first, last);
        }
    }

    rumble_args_free(parts);
    rcprintf(session, "%s OK STORE completed\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    COPY
 =======================================================================================================================
 */
ssize_t rumble_server_imap_copy(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    size_t          first,
                    last,
                    a,
                    x;
    uint32_t        useUID;
    mailman_folder  *destination = 0;
    rumble_args     *parts;
    rangePair       ranges[64];
    char            folderName[100];
    /* Check for selected folder */
    accountSession  *imap = (accountSession *) session->_svcHandle;
    mailman_folder  *folder = imap->folder;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);

    /* Is a folder selected to copy from? */
    if (!folder) {
        rcprintf(session, "%s NO COPY: I don't know where to copy from!\r\n", extra_data);
        return (RUMBLE_RETURN_IGNORE);
    }

    /* Are we using UIDs? */
    useUID = session->flags & rumble_mailman_HAS_UID;

    /* Get the destination folder */
    memset(folderName, 0, 100);
    parts = rumble_read_words(parameters);
    if (parts->argc >= 2) {
        a = strlen(parts->argv[parts->argc - 1]);
        strncpy(folderName, parts->argv[parts->argc - 1], a < 100 ? a : 99);
    }

    /* Check if folder exists */
    destination = mailman_get_folder(imap->bag, folderName);
    if (!destination) {
        rcprintf(session, "%s NO COPY [TRYCREATE] failed: Destination folder doesn't exist!\r\n", extra_data);
        return (RUMBLE_RETURN_IGNORE);
    }

    /* For each range, copy the messages */
    rumble_scan_ranges((rangePair *) &ranges, parts->argv[0]);
    rumble_args_free(parts);
    for (x = 0; ranges[x].start != 0; x++) {
        first = ranges[x].start;
        last = ranges[x].end;
        mailman_copy_letter(imap->bag, folder, destination, first, last, useUID);
    }

    rcprintf(session, "%s OK COPY completed\r\n", extra_data);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    IDLE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_idle(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            *line;
    char            buffer[5];
    int             rc = -1;
    int             cc = 0;
    int             exists = 0;
    int             recent = 0;
    int             first = 0;
    int             oexists = 0;
    int             orecent = 0;
    int             ofirst = 0;
#ifdef RUMBLE_MSC
    u_long          iMode = 1;
#endif
    mailman_letter  *letter;
    accountSession  *imap = (accountSession *) session->_svcHandle;
    mailman_folder  *folder = imap->folder;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!imap->account) return (RUMBLE_RETURN_IGNORE);
    if (!folder) {
        rcprintf(session, "%s NO No mailbox selected for fetching!\r\n", extra_data);
        return (RUMBLE_RETURN_IGNORE);
    }

    rcprintf(session, "%s OK IDLE Starting idle mode.\r\n", extra_data);
    memset(buffer, 0, 5);

    /* Retrieve the statistics of the folder before idling */
    rumble_rw_start_read(imap->bag->lock);
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (!letter->inuse) continue;
        oexists++;
        if (!ofirst && ((letter->flags & RUMBLE_LETTER_UNREAD) || (letter->flags == RUMBLE_LETTER_RECENT))) ofirst = oexists;
        if (letter->flags == RUMBLE_LETTER_RECENT) orecent++;
    }

    rumble_rw_stop_read(imap->bag->lock);

    /* While idle, check for stuff, otherwise break off */
    while (rc < 0)
    {
#ifdef RUMBLE_MSC
        ioctlsocket(session->client->socket, FIONBIO, &iMode);
        rc = recv(session->client->socket, buffer, 1, MSG_PEEK);
        iMode = 0;
        ioctlsocket(session->client->socket, FIONBIO, &iMode);
#else
        rc = recv(session->client->socket, buffer, 1, MSG_PEEK | MSG_DONTWAIT);
#endif
        if (rc == 1) break; /* got data from client again */
        else if (rc == 0) {
            printf("Idle: disconnected\n");
            return (RUMBLE_RETURN_FAILURE); /* disconnected? */
        } else if (rc == -1) {
            cc++;
            if (cc == 10) {

                /* Check the DB for new messages every 50 seconds. */
                mailman_update_folder(folder, imap->bag->uid, 0);
                cc = 0;
#ifdef RUMBLE_MSC
                rc = rcprintf(session, "* %u EXISTS\r\n", exists);  /* Testing connection :> */
                if (rc <= 0) break; /* disconnected?? */
#endif
            }

            rumble_rw_start_read(imap->bag->lock);
            for (i = 0; i < folder->size; i++) {
                letter = &folder->letters[i];
                if (!letter->inuse) continue;
                exists++;
                if (!first && ((letter->flags & RUMBLE_LETTER_UNREAD) || (letter->flags == RUMBLE_LETTER_RECENT))) first = exists;
                if (letter->flags == RUMBLE_LETTER_RECENT) recent++;
            }

            rumble_rw_stop_read(imap->bag->lock);
            if (oexists != exists) {
                rc = rcprintf(session, "* %u EXISTS\r\n", exists);
                if (rc == -1) break;
                oexists = exists;
            }

            if (recent != orecent) {
                rc = rcprintf(session, "* %u RECENT\r\n", exists);
                if (rc == -1) break;
                orecent = recent;
            }

            exists = 0;
            recent = 0;
            first = 0;
            sleep(5);
        }
    }

    line = rcread(session);
    if (!line) return (RUMBLE_RETURN_FAILURE);
    else {
        free(line);
        rcprintf(session, "%s OK IDLE completed.\r\n", extra_data);
        printf("Idle done\n");
        return (RUMBLE_RETURN_IGNORE);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_imap_logout(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {
    printf("Logging out\n");
    return (RUMBLE_RETURN_FAILURE);
}

/*
 =======================================================================================================================
    TESTING
 =======================================================================================================================
 */
ssize_t rumble_server_imap_test(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~*/
    int         x = 0;
    rangePair   ranges[64];
    /*~~~~~~~~~~~~~~~~~~~*/

    rcprintf(session, "<%s>\r\n", parameters);
    rumble_scan_ranges((rangePair *) &ranges, parameters);
    while (1) {
        if (!ranges[x].start) break;
        printf("start: %lu, stop: %lu\n", ranges[x].start, ranges[x].end);
        x++;
    }

    return (RUMBLE_RETURN_IGNORE);
}
