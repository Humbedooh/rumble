/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "servers.h"
#include "comm.h"
#include "private.h"
#include "database.h"

/*
 =======================================================================================================================
    Main loop
 =======================================================================================================================
 */
void *rumble_imap_init(void *m) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    masterHandle    *master = (masterHandle *) m;
    /* Initialize a session handle and wait for incoming connections. */
    sessionHandle   session;
    sessionHandle   *sessptr = &session;
    ssize_t         rc;
    char            *tag,
                    *cmd,
                    *arg,
                    *line,
                    *tmp;
    const char      *myName;
    int             x = 0;
    time_t          now;
    sessionHandle   *s;
    imap4Session    *pops;
    void            *pp,
                    *tp;
    pthread_t       p = pthread_self();
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    session.dict = dvector_init();
    session.recipients = dvector_init();
    session._svcHandle = (imap4Session *) malloc(sizeof(imap4Session));
    session.client = (clientHandle *) malloc(sizeof(clientHandle));
    session.client->tls = 0;
    session._master = m;
    pops = (imap4Session *) session._svcHandle;
    pops->account = 0;
    pops->bag = 0;
    pops->folder = 0;
    session._tflags = RUMBLE_THREAD_IMAP;   /* Identify the thread/session as IMAP4 */
    myName = rrdict(master->_core.conf, "servername");
    myName = myName ? myName : "??";
    tmp = (char *) malloc(100);
#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
#   ifdef PTW32_CDECL
    pp = (void *) p.p;
#   else
    pp = p;
#   endif
    printf("<imap4::threads> Initialized thread %#x\n", pp);
#endif
    while (1) {
        comm_accept(master->imap.socket, session.client);
        pthread_mutex_lock(&master->imap.mutex);
        dvector_add(master->imap.handles, (void *) sessptr);
        pthread_mutex_unlock(&master->imap.mutex);
        session.flags = 0;
        session._tflags += 0x00100000;      /* job count ( 0 through 4095) */
        session.sender = 0;
        now = time(0);
        pops->bag = 0;
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        strftime(tmp, 100, "%X", localtime(&now));
        printf("<debug::comm> [%s] Accepted connection from %s on IMAP4\n", tmp, session.client->addr);
#endif

        /* Check for hooks on accept() */
        rc = RUMBLE_RETURN_OKAY;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_IMAP);
        if (rc == RUMBLE_RETURN_OKAY) rcprintf(sessptr, "* OK <%s> IMAP4rev1 Service Ready\r\n", myName);   /* Hello! */

        /* Parse incoming commands */
        tag = (char *) malloc(32);
        cmd = (char *) malloc(32);
        arg = (char *) malloc(1024);
        if (!cmd || !arg || !tag) merror();
        while (rc != RUMBLE_RETURN_FAILURE) {
            memset(tag, 0, 32);
            memset(cmd, 0, 32);
            memset(arg, 0, 1024);
            line = rumble_comm_read(sessptr);
            rc = 421;
            if (!line) break;
            rc = 105;   /* default return code is "500 unknown command thing" */
            if (sscanf(line, "%32s %32s %1000[^\r\n]", tag, cmd, arg)) {
                rumble_string_upper(cmd);
                if (!strcmp(cmd, "UID")) {

                    /* Set UID flag if requested */
                    session.flags |= rumble_mailman_HAS_UID;
                    sscanf(arg, "%32s %1000[^\r\n]", cmd, arg);
                    rumble_string_upper(cmd);
                } else session.flags -= (session.flags & rumble_mailman_HAS_UID);   /* clear UID demand if not there. */
                printf("Client said: <%s> %s %s\r\n", tag, cmd, arg);

                /*
                 * printf("Selected folder is: %lld\r\n", pops->folder);
                 */
                if (!strcmp(cmd, "LOGIN")) rc = rumble_server_imap_login(master, &session, tag, arg);
                else if (!strcmp(cmd, "LOGOUT"))
                    break;
                else if (!strcmp(cmd, "NOOP"))
                    rc = rumble_server_imap_noop(master, &session, tag, arg);
                else if (!strcmp(cmd, "CAPABILITY"))
                    rc = rumble_server_imap_capability(master, &session, tag, arg);
                else if (!strcmp(cmd, "AUTHENTICATE"))
                    rc = rumble_server_imap_authenticate(master, &session, tag, arg);
                else if (!strcmp(cmd, "STARTTLS"))
                    rc = rumble_server_imap_starttls(master, &session, tag, arg);
                else if (!strcmp(cmd, "SELECT"))
                    rc = rumble_server_imap_select(master, &session, tag, arg);
                else if (!strcmp(cmd, "EXAMINE"))
                    rc = rumble_server_imap_examine(master, &session, tag, arg);
                else if (!strcmp(cmd, "CREATE"))
                    rc = rumble_server_imap_create(master, &session, tag, arg);
                else if (!strcmp(cmd, "DELETE"))
                    rc = rumble_server_imap_delete(master, &session, tag, arg);
                else if (!strcmp(cmd, "RENAME"))
                    rc = rumble_server_imap_rename(master, &session, tag, arg);
                else if (!strcmp(cmd, "SUBSCRIBE"))
                    rc = rumble_server_imap_subscribe(master, &session, tag, arg);
                else if (!strcmp(cmd, "UNSUBSCRIBE"))
                    rc = rumble_server_imap_unsubscribe(master, &session, tag, arg);
                else if (!strcmp(cmd, "LIST"))
                    rc = rumble_server_imap_list(master, &session, tag, arg);
                else if (!strcmp(cmd, "LSUB"))
                    rc = rumble_server_imap_lsub(master, &session, tag, arg);
                else if (!strcmp(cmd, "STATUS"))
                    rc = rumble_server_imap_status(master, &session, tag, arg);
                else if (!strcmp(cmd, "APPEND"))
                    rc = rumble_server_imap_append(master, &session, tag, arg);
                else if (!strcmp(cmd, "CHECK"))
                    rc = rumble_server_imap_check(master, &session, tag, arg);
                else if (!strcmp(cmd, "CLOSE"))
                    rc = rumble_server_imap_close(master, &session, tag, arg);
                else if (!strcmp(cmd, "EXPUNGE"))
                    rc = rumble_server_imap_expunge(master, &session, tag, arg);
                else if (!strcmp(cmd, "SEARCH"))
                    rc = rumble_server_imap_search(master, &session, tag, arg);
                else if (!strcmp(cmd, "FETCH"))
                    rc = rumble_server_imap_fetch(master, &session, tag, arg);
                else if (!strcmp(cmd, "STORE"))
                    rc = rumble_server_imap_store(master, &session, tag, arg);
                else if (!strcmp(cmd, "COPY"))
                    rc = rumble_server_imap_copy(master, &session, tag, arg);
                else if (!strcmp(cmd, "IDLE"))
                    rc = rumble_server_imap_idle(master, &session, tag, arg);
            }

            free(line);
            if (rc == RUMBLE_RETURN_IGNORE) continue;   /* Skip to next line. */
            else if (rc == RUMBLE_RETURN_FAILURE)
                break;  /* Abort! */
            else rcprintf(&session, "%s BAD Invalid command!\r\n", tag);            /* Bad command thing. */
        }

        /* Cleanup */
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        now = time(0);
        strftime(tmp, 100, "%X", localtime(&now));
        printf("<debug::comm> [%s] Closing connection from %s on IMAP4\n", tmp, session.client->addr);
#endif
        if (rc == 421) rcprintf(&session, "%s BAD Session timed out!\r\n", tag);    /* timeout! */
        else {
            rcsend(&session, "* BYE bye!\r\n");
            rcprintf(&session, "%s OK <%s> signing off for now.\r\n", tag, myName);
        }

        if (session.client->tls != NULL) comm_stoptls(&session);    /* Close the TLS session if active */
        close(session.client->socket);

        /* Start cleanup */
        free(arg);
        free(cmd);
        rumble_clean_session(sessptr);
        rumble_free_account(pops->account);
        rumble_mailman_close_bag(pops->bag);

        /* End cleanup */
        pthread_mutex_lock(&(master->imap.mutex));
        foreach((sessionHandle *), s, master->imap.handles, iter) {
            if (s == sessptr) {
                dvector_delete(&iter);
                x = 1;
                break;
            }
        }

        /* Check if we were told to go kill ourself :( */
        if (session._tflags & RUMBLE_THREAD_DIE) {

            /*~~~~~~~~~~~~*/
            pthread_t   *el;
            /*~~~~~~~~~~~~*/

#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
            printf("<imap4::threads>I (%#x) was told to die :(\n", (uintptr_t) pthread_self());
#endif
#ifdef PTW32_CDECL
            pp = (void *) p.p;
#else
            pp = p;
#endif
            foreach((pthread_t *), el, master->imap.threads, iter)
            {
#ifdef PTW32_CDECL
                tp = el->p;
#else
                tp = el;
#endif
                if (tp == pp) {
                    dvector_delete(&iter);
                    break;
                }
            }

            pthread_mutex_unlock(&master->imap.mutex);
            pthread_exit(0);
        }

        pthread_mutex_unlock(&master->imap.mutex);
    }

    pthread_exit(0);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_imap_login(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            user[512],
                    pass[512];
    address         *addr;
    imap4Session    *imap = (imap4Session *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_mailman_close_bag(imap->bag);
    if (sscanf(arg, "%s %s", user, pass) == 2) {
        addr = rumble_parse_mail_address(user);
        if (addr) {
            imap->account = rumble_account_data_auth(session, addr->user, addr->domain, pass);
            if (imap->account) {
                rcprintf(session, "%s OK Welcome!\r\n", tag);
                imap->folder = -1;
                imap->bag = rumble_mailman_open_bag(imap->account->uid);
            } else {
                rcprintf(session, "%s NO Incorrect username or password!\r\n", tag);
            }
        } else {
            rcprintf(session, "%s NO Incorrect username or password!\r\n", tag);
        }
    } else {
        rcprintf(session, "%s NO Incorrect username or password!\r\n", tag);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    NOOP
 =======================================================================================================================
 */
ssize_t rumble_server_imap_noop(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    rcprintf(session, "%s OK Doin' nothin'...\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CAPABILITY
 =======================================================================================================================
 */
ssize_t rumble_server_imap_capability(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char  *capa_plain = "* CAPABILITY IMAP4rev1 IDLE CONDSTORE UIDPLUS AUTH=PLAIN STARTTLS LOGINDISABLED\r\n";
    const char  *capa_tls = "* CAPABILITY IMAP4rev1 IDLE CONDSTORE UIDPLUS AUTH=PLAIN\r\n";
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!(session->flags & rumble_mailman_HAS_TLS)) rcsend(session, capa_plain);
    else rcsend(session, capa_tls);
    rcprintf(session, "%s OK CAPABILITY completed.\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    AUTHENTICATE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_authenticate(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    imap4Session    *imap = (imap4Session *) session->_svcHandle;
    char            method[32],
                    *user,
                    *pass,
                    *line,
                    *buffer;
    address         *addr = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_mailman_close_bag(imap->bag);
    if (sscanf(strchr(arg, '"') ? strchr(arg, '"') + 1 : arg, "%32[a-zA-Z]", method)) {
        rumble_string_upper(method);
        if (!strcmp(method, "PLAIN")) {
            rcprintf(session, "+ OK Method <%s> accepted, input stuffs!\r\n", method);
            line = rcread(session);
            if (line) {
                buffer = rumble_decode_base64(line);
                user = buffer + 1;
                pass = buffer + 2 + strlen(user);
                addr = rumble_parse_mail_address(user);
                if (addr) {
                    imap->account = rumble_account_data_auth(session, addr->user, addr->domain, pass);
                    if (imap->account) {
                        rcprintf(session, "%s OK Welcome!\r\n", tag);
                        imap->folder = -1;

                        /* Check if we have a shared mailbox instance available, if not, make one */
                        imap->bag = rumble_mailman_open_bag(imap->account->uid);
                    } else {
                        rcprintf(session, "%s NO Incorrect username or password!\r\n", tag);
                    }
                } else {
                    rcprintf(session, "%s NO Incorrect username or password!\r\n", tag);
                }

                free(buffer);
                free(line);
            }
        }
    }

    rumble_free_address(addr);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    STARTTLS
 =======================================================================================================================
 */
ssize_t rumble_server_imap_starttls(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    SELECT
 =======================================================================================================================
 */
ssize_t rumble_server_imap_select(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t                        exists,
                                    recent,
                                    first,
                                    found;
    rumble_args                     *parameters;
    rumble_mailman_shared_folder    *folder;
    char                            *selector;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    rumble_letter                   *letter;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* Are we authed? */
    if (imap->account) {
        parameters = rumble_read_words(arg);
        found = 0;
        selector = parameters->argc ? parameters->argv[0] : "";

        /* Shared Object Reader Lock */
        rumble_rw_start_read(imap->bag->rrw);
        foreach(rmsf, folder, imap->bag->folders, iter) {
            if (!strcmp(selector, folder->name)) {
                imap->folder = folder->id;
                found++;
                break;
            }
        }

        if (!found && !strcmp(selector, "INBOX")) {
            imap->folder = 0;
            found++;
        }

        if (found) {
            session->flags |= rumble_mailman_HAS_SELECT;
            session->flags |= rumble_mailman_HAS_READWRITE;
            exists = 0;
            recent = 0;
            first = 0;
            folder = rumble_mailman_current_folder(imap);
            if (!folder) {
                rcprintf(session, "%s BAD Couldn't find the mailbox <%s>!\r\n", tag, selector);
                return (RUMBLE_RETURN_IGNORE);
            }

            /* Retrieve the statistics of the folder */
            foreach((rumble_letter *), letter, folder->letters, iter) {
                exists++;
                if (!first && ((letter->flags & RUMBLE_LETTER_UNREAD) || (letter->flags == RUMBLE_LETTER_RECENT))) first = exists;
                if (letter->flags == RUMBLE_LETTER_RECENT) {
                    letter->flags |= RUMBLE_LETTER_UNREAD;
                    recent++;
                }
            }

            rcprintf(session, "* %u EXISTS\r\n", exists);
            rcsend(session, "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n");
            if (recent) rcprintf(session, "* %u RECENT.\r\n", recent);
            if (first) {
                rcprintf(session, "* OK [UNSEEN %"PRIu64 "] Message %"PRIu64 " is the first unseen message.\r\n", first, first);
            }

            rcprintf(session, "* OK [UIDVALIDITY %08u] UIDs valid\r\n", imap->account->uid);
            rcprintf(session, "%s OK [READ-WRITE] SELECT completed.\r\n", tag);
        } else {
            rcprintf(session, "%s NO No such mailbox <%s>!\r\n", tag, selector);
        }

        /* Shared Object Reader Unlock */
        rumble_rw_stop_read(imap->bag->rrw);
    } else {
        rcprintf(session, "%s NO Not logged in yet!\r\n", tag);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    EXAMINE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_examine(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t                        exists,
                                    recent,
                                    first,
                                    found;
    rumble_args                     *parameters;
    rumble_mailman_shared_folder    *folder;
    char                            *selector;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    rumble_letter                   *letter;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* Are we authed? */
    if (imap->account) {
        parameters = rumble_read_words(arg);
        found = 0;
        selector = parameters->argc ? parameters->argv[0] : "";

        /* Shared Object Reader Lock */
        rumble_rw_start_read(imap->bag->rrw);
        foreach(rmsf, folder, imap->bag->folders, iter) {
            if (!strcmp(selector, folder->name)) {
                imap->folder = folder->id;
                found++;
                break;
            }
        }

        if (!found && !strcmp(selector, "INBOX")) {
            imap->folder = 0;
            found++;
        }

        if (found) {
            session->flags |= rumble_mailman_HAS_SELECT;
            session->flags |= rumble_mailman_HAS_READWRITE;
            exists = 0;
            recent = 0;
            first = 0;
            folder = rumble_mailman_current_folder(imap);

            /* Retrieve the statistics of the folder */
            foreach((rumble_letter *), letter, folder->letters, iter) {
                exists++;
                if (!first && ((letter->flags & RUMBLE_LETTER_UNREAD) || (letter->flags == RUMBLE_LETTER_RECENT))) first = exists;
                if (letter->flags == RUMBLE_LETTER_RECENT) recent++;
            }

            rcprintf(session, "* %u EXISTS\r\n", exists);
            rcsend(session, "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n");
            if (recent) rcprintf(session, "* %u RECENT.\r\n", recent);
            if (first) rcprintf(session, "* OK [UNSEEN %u] Message %u is the first unseen message.\r\n", first, first);
            rcprintf(session, "* OK [UIDVALIDITY %08u] UIDs valid\r\n", imap->account->uid);
            rcprintf(session, "%s OK [READ-ONLY] EXAMINE completed.\r\n", tag);
        } else {
            rcprintf(session, "%s NO No such mailbox <%s>!\r\n", tag, selector);
        }

        /* Shared Object Reader Unlock */
        rumble_rw_stop_read(imap->bag->rrw);
    } else {
        rcprintf(session, "%s NO Not logged in yet!\r\n", tag);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CREATE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_create(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args                     *args;
    char                            *newName;
    void                            *state;
    rumble_mailman_shared_folder    *folder,
                                    *newFolder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    args = rumble_read_words(arg);
    newFolder = 0;
    if (args && args->argc == 1) {
        newName = args->argv[0];

        /* Shared Object Writer Lock */
        rumble_rw_start_write(imap->bag->rrw);
        foreach(rmsf, folder, imap->bag->folders, iter) {
            if (!strcmp(folder->name, newName)) {
                newFolder = folder;
                break;
            }
        }

        if (newFolder) rcprintf(session, "%s NO CREATE failed: Duplicate folder name.\r\n", tag);
        else {

            /* Add the folder to the SQL DB */
            state = rumble_database_prepare(master->_core.db, "INSERT INTO folders (uid, name) VALUES (%u, %s)", imap->account->uid, newName);
            rumble_database_run(state);
            rumble_database_cleanup(state);

            /* Update the local folder list */
            rumble_mailman_update_folders(imap->bag);
            rcprintf(session, "%s OK CREATE completed\r\n", tag);
        }

        /* Shared Object Writer Unlock */
        rumble_rw_stop_write(imap->bag->rrw);
    } else rcprintf(session, "%s BAD Invalid CREATE syntax!\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    DELETE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_delete(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    RENAME
 =======================================================================================================================
 */
ssize_t rumble_server_imap_rename(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args                     *args;
    char                            *oldName,
                                    *newName;
    void                            *state;
    rumble_mailman_shared_folder    *folder,
                                    *oldFolder,
                                    *newFolder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    args = rumble_read_words(arg);
    oldFolder = 0;
    newFolder = 0;
    if (args && args->argc == 2) {
        oldName = args->argv[0];
        newName = args->argv[1];

        /* Shared Object Writer Lock */
        rumble_rw_start_write(imap->bag->rrw);
        foreach(rmsf, folder, imap->bag->folders, iter) {
            if (!strcmp(folder->name, oldName)) oldFolder = folder;
            if (!strcmp(folder->name, newName)) newFolder = folder;
        }

        if (newFolder) rcprintf(session, "%s NO RENAME failed: Duplicate folder name.\r\n", tag);
        else if (!oldFolder)
            rcprintf(session, "%s NO RENAME failed: No such folder <%s>\r\n", tag, oldName);
        else {
            state = rumble_database_prepare(master->_core.db, "UPDATE folders set name = %s WHERE id = %u", newName, oldFolder->id);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            free(oldFolder->name);
            oldFolder->name = (char *) calloc(1, strlen(newName) + 1);
            strncpy(oldFolder->name, newName, strlen(newName));
            rcprintf(session, "%s OK RENAME completed\r\n", tag);
            oldFolder->updated = time(0);
        }

        /* Shared Object Writer Unlock */
        rumble_rw_stop_write(imap->bag->rrw);
    } else rcprintf(session, "%s BAD Invalid RENAME syntax!\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    SUBSCRIBE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_subscribe(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args                     *args;
    char                            *folderName;
    void                            *state;
    rumble_mailman_shared_folder    *pair,
                                    *folder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    args = rumble_read_words(arg);
    folder = 0;
    if (args && args->argc == 1) {
        folderName = args->argv[0];

        /* Shared Object Writer Lock */
        rumble_rw_start_write(imap->bag->rrw);
        foreach(rmsf, pair, imap->bag->folders, iter) {
            if (!strcmp(pair->name, folderName)) folder = pair;
        }

        if (!folder) rcprintf(session, "%s NO SUBSCRIBE failed: No such folder <%s>\r\n", tag, folderName);
        else {
            state = rumble_database_prepare(master->_core.db, "UPDATE folders set subscribed = true WHERE id = %u", folder->id);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            folder->subscribed = 1;
            rcprintf(session, "%s OK SUBSCRIBE completed\r\n", tag);
        }

        /* Shared Object Writer Unlock */
        rumble_rw_stop_write(imap->bag->rrw);
    } else rcprintf(session, "%s BAD Invalid SUBSCRIBE syntax!\r\n", tag);
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    UNSUBSCRIBE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_unsubscribe(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args                     *args;
    char                            *folderName;
    void                            *state;
    rumble_mailman_shared_folder    *pair,
                                    *folder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    args = rumble_read_words(arg);
    folder = 0;
    if (args && args->argc == 1) {
        folderName = args->argv[0];

        /* Shared Object Writer Lock */
        rumble_rw_start_write(imap->bag->rrw);
        foreach(rmsf, pair, imap->bag->folders, iter) {
            if (!strcmp(pair->name, folderName)) folder = pair;
        }

        if (!folder) rcprintf(session, "%s NO UNSUBSCRIBE failed: No such folder <%s>\r\n", tag, folderName);
        else {
            state = rumble_database_prepare(master->_core.db, "UPDATE folders set subscribed = false WHERE id = %u", folder->id);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            folder->subscribed = 0;
            rcprintf(session, "%s OK UNSUBSCRIBE completed\r\n", tag);
        }

        /* Shared Object Writer Unlock */
        rumble_rw_stop_write(imap->bag->rrw);
    } else rcprintf(session, "%s BAD Invalid UNSUBSCRIBE syntax!\r\n", tag);
    rumble_args_free(args);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    LIST
 =======================================================================================================================
 */
ssize_t rumble_server_imap_list(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args                     *args;
    char                            *mbox,
                                    *pattern,
                                    *pfolder;
    rumble_mailman_shared_folder    *pair;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    args = rumble_read_words(arg);
    if (args && args->argc == 2) {
        mbox = args->argv[0];
        pattern = args->argv[1];

        /* Shared Object Reader Lock */
        rumble_rw_start_read(imap->bag->rrw);
        if (imap->folder == -1) {
            rcsend(session, "* LIST (\\Noselect) \".\" \"\"\r\n");
            rcsend(session, "* LIST () \".\" \"INBOX\"\r\n");
        }

        if (imap->folder != -1) {
            foreach(rmsf, pair, imap->bag->folders, iter) {
                if (pair->id == imap->folder) pfolder = pair->name;
                break;
            }
        }

        foreach(rmsf, pair, imap->bag->folders, iter) {
            rcprintf(session, "* LIST () \".\" \"%s\"\r\n", pair->name);
        }

        /* Shared Object Reader Unlock */
        rumble_rw_stop_read(imap->bag->rrw);
        rcprintf(session, "%s OK LIST completed\r\n", tag);
    } else rcprintf(session, "%s BAD Invalid LIST syntax!\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    LSUB
 =======================================================================================================================
 */
ssize_t rumble_server_imap_lsub(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_args                     *args;
    char                            *mbox,
                                    *pattern,
                                    *pfolder;
    rumble_mailman_shared_folder    *pair;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    args = rumble_read_words(arg);
    if (args && args->argc == 2) {
        mbox = args->argv[0];
        pattern = args->argv[1];

        /* Shared Object Reader Lock */
        rumble_rw_start_read(imap->bag->rrw);
        if (imap->folder != -1) {
            foreach(rmsf, pair, imap->bag->folders, iter) {
                if (pair->id == imap->folder) pfolder = pair->name;
                break;
            }
        }

        foreach(rmsf, pair, imap->bag->folders, iter) {
            if (pair->subscribed) rcprintf(session, "* LSUB () \".\" \"%s\"\r\n", pair->name);
        }

        /* Shared Object Reader Unlock */
        rumble_rw_stop_read(imap->bag->rrw);
        rcprintf(session, "%s OK LSUB completed\r\n", tag);
    } else rcprintf(session, "%s BAD Invalid LSUB syntax!\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    STATUS
 =======================================================================================================================
 */
ssize_t rumble_server_imap_status(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    APPEND
 =======================================================================================================================
 */
ssize_t rumble_server_imap_append(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    rcprintf(session, "%s NO Append completed\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CHECK
 =======================================================================================================================
 */
ssize_t rumble_server_imap_check(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    CLOSE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_close(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_mailman_shared_folder    *folder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    folder = rumble_mailman_current_folder(imap);
    if (!folder) {
        rcprintf(session, "%s NO CLOSE: No mailbox to close!\r\n", tag);
        return (RUMBLE_RETURN_IGNORE);
    }

    if (imap->account && (session->flags & rumble_mailman_HAS_SELECT)) {
        rumble_mailman_commit(imap, folder);
        session->flags -= rumble_mailman_HAS_SELECT;    /* clear select flag. */
        imap->folder = -1;
        rcprintf(session, "%s OK Expunged and closed the mailbox.\r\n", tag);
    } else rcprintf(session, "%s NO CLOSE: No mailbox to close!\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    EXPUNGE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_expunge(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    rumble_mailman_shared_folder    *folder = rumble_mailman_current_folder(imap);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (imap->account && (session->flags & rumble_mailman_HAS_SELECT)) {
        rumble_mailman_commit(imap, folder);
        rcprintf(session, "%s OK Expunged them letters.\r\n", tag);
    } else rcprintf(session, "%s NO EXPUNGE: No mailbox selected for expunging!\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    SEARCH
 =======================================================================================================================
 */
ssize_t rumble_server_imap_search(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    FETCH
 =======================================================================================================================
 */
ssize_t rumble_server_imap_fetch(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    rumble_args                     *parts;
    rumble_mailman_shared_folder    *folder;
    int                             a,
                                    b,
                                    c,
                                    w_uid,
                                    first,
                                    last;
    char                            line[1024];
    const char                      *body,
                                    *body_peek;
    int                             flags,
                                    uid,
                                    internaldate,
                                    envelope;
    int                             size,
                                    text,
                                    header; /* rfc822.size/text/header */
    uint32_t                        octets;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    folder = rumble_mailman_current_folder(imap);
    if (!folder) {
        printf("%s NO No mailbox selected for fetching! <%s>\r\n", tag, arg);
        rcprintf(session, "%s NO No mailbox selected for fetching!\r\n", tag);
        return (RUMBLE_RETURN_IGNORE);
    }

    uid = strstr(arg, "UID") ? 1 : 0;
    internaldate = strstr(arg, "INTERNALDATE") ? 1 : 0;
    envelope = strstr(arg, "ENVELOPE") ? 1 : 0;
    size = strstr(arg, "RFC822.SIZE") ? 1 : 0;
    text = strstr(arg, "RFC822.TEXT") ? 1 : 0;
    header = strstr(arg, "RFC822.HEADER") ? 1 : 0;
    flags = strstr(arg, "FLAGS") ? 1 : 0;
    octets = 0;
    memset(line, 0, 1024);
    body_peek = strstr(arg, "BODY.PEEK[");
    body = strstr(arg, "BODY[");
    parts = 0;
    first = 0;
    last = 0;
    if (sscanf(arg, "%u:%c[*]", &first, (char *) &last) == 2) {
        last = -1;
    } else sscanf(arg, "%u:%u", &first, &last);
    if (last == 0) last = first;
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
            printf("%u %s\r\n", octets, line);
            if (sscanf(line, "%32s (%1000c)", region, buffer) == 2) {
                parts = rumble_read_words(buffer);
                for (b = 0; b < parts->argc; b++) rumble_string_lower(parts->argv[b]);
            }
        }
    }

    b = 0;
    a = 0;
    foreach((rumble_letter *), letter, folder->letters, iter) {
        a++;
        if (w_uid && (letter->id < first || (last > 0 && letter->id > last))) continue;
        if (!w_uid && (a < first || (last > 0 && a > last))) continue;
        b++;
        rcprintf(session, "* %u FETCH (", a + 1);
        if (flags) {
            rcprintf(session, "FLAGS (%s%s%s%s) ", (letter->flags == RUMBLE_LETTER_RECENT) ? "\\Recent " : "",
                     (letter->flags & RUMBLE_LETTER_READ) ? "\\Seen " : "", (letter->flags & RUMBLE_LETTER_DELETED) ? "\\Deleted " : "",
                     (letter->flags & RUMBLE_LETTER_FLAGGED) ? "\\Flagged " : "");
        }

        if (uid || w_uid) rcprintf(session, "UID %u ", letter->id);
        if (size) rcprintf(session, "RFC822.SIZE %u ", letter->size);
        if (internaldate) rcprintf(session, "INTERNALDATE %u ", letter->delivered);
        if (body) letter->flags -= (letter->flags & RUMBLE_LETTER_RECENT);  /* Remove \Recent flag since we're not peeking. */
        if (body || body_peek) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char    header[10240],
                    key[64];
            FILE    *fp = rumble_letters_open(imap->account, letter);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
                                 * }
                                 */
                                strncpy(header + strlen(header), line, strlen(line));
                            }
                        }
                    }

                    sprintf(header + strlen(header), "\r\n \r\n");
                    rcprintf(session, "BODY[HEADER.FIELDS (%s)] {%u}\r\n", line, strlen(header));
                    rcsend(session, header);
                    printf("BODY[HEADER.FIELDS (%s)] {%u}\r\n", line, strlen(header));
                    printf("%s", header);
                } else {
                    rcprintf(session, "BODY[] {%u}\r\n", letter->size);
                    printf("BODY[] {%u}\r\n", letter->size);
                    memset(line, 0, 1024);
                    while (fgets(line, 1024, fp)) {
                        rcsend(session, line);
                        printf("%s", line);
                    }
                }

                fclose(fp);
            } else printf("meh, couldn't open letter file\n");
            rcsend(session, " ");
        }

        rcprintf(session, ")\r\n");
    }

    rcprintf(session, "%s OK FETCH completed\r\n", tag);
    printf("Fetched %u letters\n", b);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    STORE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_store(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                             first,
                                    last,
                                    silent,
                                    control,
                                    a,
                                    useUID,
                                    flag;
    rumble_letter                   *letter;
    char                            args[100],
                                    smurf[4];
    /* Check for selected folder */
    rumble_mailman_shared_folder    *folder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    folder = rumble_mailman_current_folder(imap);
    if (!folder) {
        rcprintf(session, "%s NO STORE: No mailbox selected for storing!\r\n", tag);
        return (RUMBLE_RETURN_IGNORE);
    }

    /* Get the message range */
    first = 0;
    last = 0;
    if (sscanf(arg, "%u:%1[*]", &first, (char *) &smurf) == 2) {
        last = -1;
    } else sscanf(arg, "%u:%u", &first, &last);
    if (last == 0) last = first;
    useUID = session->flags & rumble_mailman_HAS_UID;

    /* Get the store type */
    silent = strstr(arg, ".SILENT") ? 1 : 0;
    control = strchr(arg, '+') ? 1 : (strchr(arg, '-') ? -1 : 0);
    memset(args, 0, 100);
    sscanf(arg, "%*100[^(](%99[^)])", args);

    /* Set the master flag */
    flag = 0;
    flag |= strstr(arg, "\\Deleted") ? RUMBLE_LETTER_DELETED : 0;
    flag |= strstr(arg, "\\Seen") ? RUMBLE_LETTER_READ : 0;
    flag |= strstr(arg, "\\Flagged") ? RUMBLE_LETTER_FLAGGED : 0;
    flag |= strstr(arg, "\\Draft") ? RUMBLE_LETTER_DRAFT : 0;
    flag |= strstr(arg, "\\Answered") ? RUMBLE_LETTER_ANSWERED : 0;

    /* Process the letters */
    a = 0;
    foreach((rumble_letter *), letter, folder->letters, iter) {
        a++;
        if (useUID && (letter->id < first || (last > 0 && letter->id > last))) continue;
        if (!useUID && (a < first || (last > 0 && a > last))) continue;

        /* +FLAGS ? */
        if (control == 1) letter->flags |= flag;

        /* -FLAGS ? */
        if (control == -1) letter->flags -= (flag & letter->flags);

        /* FLAGS ? */
        if (control == 0) letter->flags = (flag | RUMBLE_LETTER_UNREAD);
        if (!silent) {
            rcprintf(session, "* %u FETCH (FLAGS (%s%s%s%s))\r\n", (a + 1), (letter->flags == RUMBLE_LETTER_RECENT) ? "\\Recent " : "",
                     (letter->flags & RUMBLE_LETTER_READ) ? "\\Seen " : "", (letter->flags & RUMBLE_LETTER_DELETED) ? "\\Deleted " : "",
                     (letter->flags & RUMBLE_LETTER_FLAGGED) ? "\\Flagged " : "");
        }
    }

    rcprintf(session, "%s OK STORE completed\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    COPY
 =======================================================================================================================
 */
ssize_t rumble_server_imap_copy(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                             first,
                                    last,
                                    a,
                                    b,
                                    useUID;
    int64_t                         destination;
    rumble_letter                   *letter;
    rumble_args                     *parts;
    char                            folderName[100],
                                    *filename,
                                    path[256],
                                    *opath;
    FILE                            *in,
                                    *out;
    /* Check for selected folder */
    rumble_mailman_shared_folder    *folder;
    imap4Session                    *imap = (imap4Session *) session->_svcHandle;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    folder = rumble_mailman_current_folder(imap);
    if (!folder) {
        rcprintf(session, "%s NO COPY: I don't know where to copy from!\r\n", tag);
        return (RUMBLE_RETURN_IGNORE);
    }

    destination = -1;

    /* Get the message range */
    first = 0;
    last = 0;
    if (sscanf(arg, "%u:%1[*]", &first, (char *) &last) == 2) {
        last = -1;
    } else sscanf(arg, "%u:%u", &first, &last);
    if (last == 0) last = first;
    useUID = session->flags & rumble_mailman_HAS_UID;

    /* Get the destination folder */
    memset(folderName, 0, 100);
    parts = rumble_read_words(arg);
    if (parts->argc >= 2) {
        a = strlen(parts->argv[1]);
        strncpy(folderName, parts->argv[1], a < 100 ? a : 99);
    }

    /* Check if folder exists */
    foreach(rmsf, folder, imap->bag->folders, iter) {
        if (!strcmp(folderName, folder->name)) {
            destination = folder->id;
            break;
        }
    }

    if (!destination && !strcmp(folderName, "INBOX")) destination = 0;

    /* Copy them letters */
    folder = rumble_mailman_current_folder(imap);
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_STORAGE)
    printf("Copying letters %u through %u (UID = %s) to %lld...\n", first, last, useUID ? "enabled" : "disabled", destination);
    printf("Folder has %u letters\n", folder->letters->size);
#endif
    opath = (char *) (strlen(imap->account->domain->path) ? imap->account->domain->path : rrdict(master->_core.conf, "storagefolder"));
    if (destination != -1) {

        /*~~~~~~~~~~~~~~~~~*/
        char    buffer[4096];
        void    *state;
        /*~~~~~~~~~~~~~~~~~*/

        a = 0;
        foreach((rumble_letter *), letter, folder->letters, iter) {
            a++;
            if (useUID && (letter->id < first || (last > 0 && letter->id > last))) continue;
            if (!useUID && (a < first || (last > 0 && a > last))) continue;
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_STORAGE)
            printf("Copying letter %llu...\n", letter->id);
#endif
            filename = rumble_create_filename();
            sprintf(path, "%s/%s.msg", opath, filename);
            in = rumble_letters_open(imap->account, letter);
            if (!in) {
                printf("couldn't open in-file <%s>\r\n", letter->fid);
                continue;
            }

            out = fopen(path, "wb");
            if (!in) {
                fclose(in);
                printf("couldn't open out-file <%s>\r\n", path);
                continue;
            }

            while (!feof(in)) {
                b = fread(buffer, 1, 4096, in);
                fwrite(buffer, 1, b, out);
            }

            fclose(in);
            fclose(out);
            state = rumble_database_prepare(master->_core.db,
                                            "INSERT INTO mbox (uid, fid, folder, size, flags) VALUES (%u, %s, %u, %u, %u)", imap->account->uid,
                                            filename, destination, letter->size, 0);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            free(filename);
        }

        rcprintf(session, "%s OK COPY completed\r\n", tag);
    } else {
        rcprintf(session, "%s NO COPY [TRYCREATE] failed: Destination folder doesn't exist!\r\n", tag);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
    IDLE
 =======================================================================================================================
 */
ssize_t rumble_server_imap_idle(masterHandle *master, sessionHandle *session, const char *tag, const char *arg) {

    /*~~~~~~~~~~*/
    char    *line;
    /*~~~~~~~~~~*/

    rcprintf(session, "+ OK IDLE Starting idle mode.\r\n", tag);
    line = rcread(session);
    rcprintf(session, "+ OK IDLE completed.\r\n", tag);
    return (RUMBLE_RETURN_IGNORE);
}
