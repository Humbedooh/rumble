/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "servers.h"
#include "comm.h"
#include "reply_codes.h"
#include "private.h"

/*
 =======================================================================================================================
    Main loop
 =======================================================================================================================
 */
void *rumble_pop3_init(void *T) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleThread    *thread = (rumbleThread *) T;
    rumbleService   *svc = thread->svc;
    masterHandle    *master = svc->master;
    /* Initialize a session handle and wait for incoming connections. */
    sessionHandle   session;
    sessionHandle   *sessptr = &session;
    ssize_t         rc;
    char            *cmd,
                    *arg,
                    *line;
    const char      *myName;
    int             x = 0;
    sessionHandle   *s;
    accountSession  *pops;
    d_iterator      iter;
    c_iterator      citer;
    svcCommandHook  *hook;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    session.dict = dvector_init();
    session.recipients = dvector_init();
    session._svcHandle = (accountSession *) malloc(sizeof(accountSession));
    session.client = (clientHandle *) malloc(sizeof(clientHandle));
    session._master = svc->master;
    session._svc = svc;
    pops = (accountSession *) session._svcHandle;
    pops->account = 0;
    pops->bag = 0;
    session._tflags = RUMBLE_THREAD_POP3;   /* Identify the thread/session as POP3 */
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
        session._svc = svc;
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        rumble_debug("pop3", "Accepted connection from %s on POP3", session.client->addr);
#endif

        /* Check for hooks on accept() */
        rc = RUMBLE_RETURN_OKAY;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_POP3);
        if (rc == RUMBLE_RETURN_OKAY) rcprintf(sessptr, rumble_pop3_reply_code(101), myName);   /* Hello! */
        else svc->traffic.rejections++;

        /* Parse incoming commands */
        cmd = (char *) malloc(9);
        arg = (char *) malloc(1024);
        if (!cmd || !arg) merror();
        while (rc != RUMBLE_RETURN_FAILURE) {
            memset(cmd, 0, 9);
            memset(arg, 0, 1024);
            line = rumble_comm_read(sessptr);
            rc = 421;
            if (!line) break;
            rc = 105;   /* default return code is "500 unknown command thing" */
            if (sscanf(line, "%8[^\t \r\n]%*[ \t]%1000[^\r\n]", cmd, arg)) {
                rumble_string_upper(cmd);

                /*
                 * rumble_debug("pop3", "%s said: %s %s", session.client->addr, cmd, arg);
                 */
                if (!strcmp(cmd, "QUIT")) {
                    rc = RUMBLE_RETURN_FAILURE;
                    break;
                }       /* bye! */

                cforeach((svcCommandHook *), hook, svc->commands, citer) {
                    if (!strcmp(cmd, hook->cmd)) rc = hook->func(master, &session, arg, 0);
                }
            }

            free(line);
            if (rc == RUMBLE_RETURN_IGNORE) continue;   /* Skip to next line. */
            else if (rc == RUMBLE_RETURN_FAILURE) {
                svc->traffic.rejections++;
                break;  /* Abort! */
            }
            else rumble_comm_send(sessptr, rumble_pop3_reply_code(rc));         /* Bad command thing. */
        }

        /* Cleanup */
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        rumble_debug("pop3", "Closing connection from %s on POP3", session.client->addr);
#endif
        if (rc == 421) rumble_comm_send(sessptr, rumble_pop3_reply_code(103));  /* timeout! */
        else rumble_comm_send(sessptr, rumble_pop3_reply_code(102));            /* bye! */

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Close socket and run pre-close hooks.
         ---------------------------------------------------------------------------------------------------------------
         */

        rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_CLOSE + RUMBLE_HOOK_POP3);
        disconnect(session.client->socket);

        /* Start cleaning up after the session */
        free(arg);
        free(cmd);
        rumble_clean_session(sessptr);
        rumble_mailman_commit(pops, rumble_mailman_current_folder(pops), 1);    /* Delete letters marked "expunged" to prevent IMAP mixup */
        rumble_free_account(pops->account);
        rumble_mailman_close_bag(pops->bag);

        /* Update the thread stats */
        pthread_mutex_lock(&(svc->mutex));
        foreach((sessionHandle *), s, svc->handles, iter) {
            if (s == sessptr) {
                dvector_delete(&iter);
                x = 1;
                break;
            }
        }

        /* Check if we were told to go kill ourself :( */
        if ((session._tflags & RUMBLE_THREAD_DIE) || svc->enabled != 1 || thread->status == -1) {

            /*~~~~~~~~~~~~~~~*/
            rumbleThread    *t;
            /*~~~~~~~~~~~~~~~*/

#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
            printf("<pop3::threads>I (%#x) was told to die :(\n", (uintptr_t) pthread_self());
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
    }

    pthread_exit(0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_capa(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~*/
    char        *el;
    c_iterator  iter;
    /*~~~~~~~~~~~~~*/

    rcsend(session, "+OK Here's what I got:\r\n");
    cforeach((char *), el, ((rumbleService *) session->_svc)->capabilities, iter) {
        rcprintf(session, "%s\r\n", el);
    }

    rcsend(session, ".\r\n");
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_user(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {
    if (session->flags & RUMBLE_POP3_HAS_AUTH) return (105);
    if (!strlen(parameters)) return (107);  /* invalid syntax */
    rfdict(session->dict);
    rsdict(session->dict, "user", parameters);
    session->flags |= RUMBLE_POP3_HAS_USER;
    return (104);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_pass(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            usr[128],
                    dmn[128],
                    *tmp;
    int             n = 0;
    accountSession  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!strlen(parameters)) return (107);
    if (!(session->flags & RUMBLE_POP3_HAS_USER)) return (105);
    if (session->flags & RUMBLE_POP3_HAS_AUTH) return (105);
    memset(usr, 0, 128);
    memset(dmn, 0, 128);
    if (sscanf(rrdict(session->dict, "user"), "%127[^@]@%127c", usr, dmn) == 2) {
        rumble_debug("pop3", "%s requested access to %s@%s\n", session->client->addr, usr, dmn);
        if ((pops->account = rumble_account_data(0, usr, dmn))) {
            tmp = rumble_sha256((const unsigned char *) parameters);
            n = strcmp(tmp, pops->account->hash);
            free(tmp);
            if (n) {
                rumble_debug("pop3", "%s's request for %s@%s was denied (wrong password)\n", session->client->addr, usr, dmn);
                rumble_free_account(pops->account);
                free(pops->account);
                pops->account = 0;
                return (106);
            } else {
                rumble_debug("pop3", "%s's request for %s@%s was granted\n", session->client->addr, usr, dmn);
                session->flags |= RUMBLE_POP3_HAS_AUTH;
                pops->bag = rumble_mailman_open_bag(pops->account->uid);
                pops->folder = 0;   /* 0 = INBOX */
                return (104);
            }
        }
    }

    return (106);   /* bad user/pass given. */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_list(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    uint32_t                        i;
    rumble_mailman_shared_folder    *folder;
    d_iterator                      iter;
    accountSession                  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!(session->flags & RUMBLE_POP3_HAS_AUTH)) return (105); /* Not authed?! :( */
    rcsend(session, "+OK\r\n");
    rumble_rw_start_read(pops->bag->rrw);
    folder = rumble_mailman_current_folder(pops);
    i = 0;
    foreach((rumble_letter *), letter, folder->letters, iter) {
        i++;
        if (!(letter->flags & RUMBLE_LETTER_DELETED)) rcprintf(session, "%u %u\r\n", i, letter->size);
    }

    rumble_rw_stop_read(pops->bag->rrw);
    rcsend(session, ".\r\n");
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_stat(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    uint32_t                        n,
                                    s;
    rumble_mailman_shared_folder    *folder;
    d_iterator                      iter;
    accountSession                  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("Doing stat\n");
    if (!(session->flags & RUMBLE_POP3_HAS_AUTH)) return (105); /* Not authed?! :( */
    rumble_rw_start_read(pops->bag->rrw);
    folder = rumble_mailman_current_folder(pops);
    if (!folder) {
        rcsend(session, "-ERR Temporary error\r\n");
        return (RUMBLE_RETURN_IGNORE);
    }

    n = 0;
    s = 0;
    printf("summing up\n");
    foreach((rumble_letter *), letter, folder->letters, iter) {
        n++;
        if (!(letter->flags & RUMBLE_LETTER_DELETED)) s += letter->size;
    }

    rumble_rw_stop_read(pops->bag->rrw);
    rcprintf(session, "+OK %u %u\r\n", n, s);
    printf("stat done\n");
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_uidl(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    uint32_t                        i;
    rumble_mailman_shared_folder    *folder;
    d_iterator                      iter;
    accountSession                  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!(session->flags & RUMBLE_POP3_HAS_AUTH)) return (105); /* Not authed?! :( */
    rcsend(session, "+OK\r\n");
    rumble_rw_start_read(pops->bag->rrw);
    folder = rumble_mailman_current_folder(pops);
    i = 0;
    foreach((rumble_letter *), letter, folder->letters, iter) {
        i++;
        if (!(letter->flags & RUMBLE_LETTER_DELETED)) rcprintf(session, "%u %s\r\n", i, letter->fid);
    }

    rumble_rw_stop_read(pops->bag->rrw);
    rcsend(session, ".\r\n");
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_dele(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    rumble_mailman_shared_folder    *folder;
    int                             j,
                                    i,
                                    found;
    d_iterator                      iter;
    accountSession                  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!(session->flags & RUMBLE_POP3_HAS_AUTH)) return (105); /* Not authed?! :( */
    i = atoi(parameters);
    found = 0;
    rumble_rw_start_write(pops->bag->rrw);
    folder = rumble_mailman_current_folder(pops);
    j = 0;
    foreach((rumble_letter *), letter, folder->letters, iter) {
        j++;
        if (j == i) {
            letter->flags |= RUMBLE_LETTER_EXPUNGE; /* Used to be _DELETED, but that was baaad. */
            found = 1;
            break;
        }
    }

    rumble_rw_stop_write(pops->bag->rrw);
    if (found) rcsend(session, "+OK\r\n");
    else rcsend(session, "-ERR No such letter.\r\n");
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_retr(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    char                            buffer[2049];
    FILE                            *fp;
    int                             j,
                                    i,
                                    found;
    d_iterator                      iter;
    rumble_mailman_shared_folder    *folder;
    accountSession                  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    fp = 0;
    if (!(session->flags & RUMBLE_POP3_HAS_AUTH)) return (105); /* Not authed?! :( */
    i = atoi(parameters);
    found = 0;
    rumble_rw_start_read(pops->bag->rrw);
    folder = rumble_mailman_current_folder(pops);
    j = 0;
    foreach((rumble_letter *), letter, folder->letters, iter) {
        j++;
        if (j == i) {
            fp = rumble_letters_open(pops->account, letter);
            found = 1;
            break;
        }
    }

    rumble_rw_stop_read(pops->bag->rrw);
    if (fp) {
        rcsend(session, "+OK\r\n");
        while (!feof(fp)) {
            if (!fgets(buffer, 2048, fp)) break;
            rcsend(session, buffer);
        }

        fclose(fp);
        rcsend(session, "\r\n.\r\n");
    } else {
        rcprintf(session, "-ERR Couldn't open letter no. %d.\r\n", i);
        /* Might as well delete the letter if it doesn't exist :( */
        rumble_rw_start_write(pops->bag->rrw);
        folder = rumble_mailman_current_folder(pops);
        j = 0;
        foreach((rumble_letter *), letter, folder->letters, iter) {
            j++;
            if (j == i) {
                letter->flags |= RUMBLE_LETTER_EXPUNGE; /* Used to be _DELETED, but that was baaad. */
                found = 1;
                break;
            }
        }

        rumble_rw_stop_write(pops->bag->rrw);
    }

    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_pop3_top(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char                            buffer[2049];
    FILE                            *fp;
    int                             i,
                                    found,
                                    lines,
                                    j;
    rumble_mailman_shared_folder    *folder;
    rumble_letter                   *letter;
    d_iterator                      iter;
    accountSession                  *pops = (accountSession *) session->_svcHandle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    fp = 0;
    if (!(session->flags & RUMBLE_POP3_HAS_AUTH)) return (105); /* Not authed?! :( */
    if (sscanf(parameters, "%i %i", &i, &lines) == 2) {
        found = 0;
        rumble_rw_start_read(pops->bag->rrw);
        folder = rumble_mailman_current_folder(pops);
        j = 0;
        foreach((rumble_letter *), letter, folder->letters, iter) {
            j++;
            if (j == i) {
                fp = rumble_letters_open(pops->account, letter);
                found = 1;
                break;
            }
        }

        rumble_rw_stop_read(pops->bag->rrw);
        if (fp) {
            rcsend(session, "+OK\r\n");
            while (!feof(fp) && lines) {
                lines--;
                if (!fgets(buffer, 2048, fp)) break;
                rcsend(session, buffer);
            }

            fclose(fp);
            rcsend(session, ".\r\n");
        } else {
            rcprintf(session, "-ERR Couldn't open letter no. %d.\r\n", i);
        }

        return (RUMBLE_RETURN_IGNORE);
    }

    return (105);
}
