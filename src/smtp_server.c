/*$I0 */
#include "rumble.h"
#include "servers.h"
#include "sqlite3.h"
#include "comm.h"
#include "private.h"
#include "database.h"

/*
 =======================================================================================================================
    Main loop
 =======================================================================================================================
 */
void *rumble_smtp_init(void *m) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    masterHandle    *master = (masterHandle *) m;
    /* Initialize a session handle and wait for incoming connections. */
    sessionHandle   session;
    sessionHandle   *sessptr = &session;
    ssize_t         rc;
    char            *line;
    char            cmd[10],
                    arg[1024],
                    tmp[100];
    const char      *myName;
    int             x = 0;
    time_t          now;
    sessionHandle   *s;
    void            *pp,
                    *tp;
    pthread_t       p = pthread_self();
    d_iterator      iter;
    c_iterator      citer;
    svcCommandHook  *hook;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_rw_start_read(master->domains.rrw);
    session.dict = dvector_init();
    session.recipients = dvector_init();
    session.client = (clientHandle *) malloc(sizeof(clientHandle));
    session.client->tls = 0;
    session.client->recv = 0;
    session.client->send = 0;
    session._master = m;
    session._tflags = RUMBLE_THREAD_SMTP;   /* Identify the thread/session as SMTP */
    myName = rrdict(master->_core.conf, "servername");
    myName = myName ? myName : "??";
    rumble_rw_stop_read(master->domains.rrw);
#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
#   ifdef PTW32_CDECL
    pp = (void *) p.p;
#   else
    pp = p;
#   endif
    printf("<smtp::threads> Initialized thread %#x\n", pp);
#endif
    while (1) {
        comm_accept(master->smtp.socket, session.client);
        pthread_mutex_lock(&master->smtp.mutex);
        dvector_add(master->smtp.handles, (void *) sessptr);
        pthread_mutex_unlock(&master->smtp.mutex);
        session.flags = 0;
        session._tflags += 0x00100000;      /* job count ( 0 through 4095) */
        session.sender = 0;
        now = time(0);
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        strftime(tmp, 100, "%X", localtime(&now));
        printf("<debug::comm> [%s] Accepted connection from %s on SMTP\n", tmp, session.client->addr);
#endif

        /* Check for hooks on accept() */
        rc = RUMBLE_RETURN_OKAY;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_SMTP);
        if (rc == RUMBLE_RETURN_OKAY) rcprintf(sessptr, rumble_smtp_reply_code(220), myName);   /* Hello! */
        else printf("hrmmm %d\n", rc);
        /* Parse incoming commands */
        if (!cmd || !arg) merror();
        while (rc != RUMBLE_RETURN_FAILURE) {
            memset(cmd, 0, 10);
            memset(arg, 0, 1024);
            line = rumble_comm_read(sessptr);
            rc = 421;
            if (!line) break;
            rc = 500;   /* default return code is "500 unknown command thing" */
            if (sscanf(line, "%8[^\t \r\n]%*[ \t]%1000[^\r\n]", cmd, arg)) {
                rumble_string_upper(cmd);
                if (!strcmp(cmd, "QUIT")) {
                    rc = RUMBLE_RETURN_FAILURE;
                } /* bye! */ else {
                    cforeach((svcCommandHook *), hook, master->smtp.commands, citer) {
                        if (!strcmp(cmd, hook->cmd)) rc = hook->func(master, &session, arg, 0);
                    }
                }
            }

            free(line);
            if (rc == RUMBLE_RETURN_IGNORE) continue;   /* Skip to next line. */
            else if (rc == RUMBLE_RETURN_FAILURE)
                break;  /* Abort! */
            else rumble_comm_send(sessptr, rumble_smtp_reply_code(rc)); /* Bad command thing. */
        }

        /* Cleanup */
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        now = time(0);
        strftime(tmp, 100, "%X", localtime(&now));
        printf("<debug::comm> [%s] Closing connection from %s on SMTP\n", tmp, session.client->addr);
#endif
        if (rc == 421) rumble_comm_send(sessptr, rumble_smtp_reply_code(421422));   /* timeout! */
        else rumble_comm_send(sessptr, rumble_smtp_reply_code(221220)); /* bye! */

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Close socket and run pre-close hooks.
         ---------------------------------------------------------------------------------------------------------------
         */

        rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_CLOSE + RUMBLE_HOOK_IMAP);
        close(session.client->socket);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Clean up after the session
         ---------------------------------------------------------------------------------------------------------------
         */

        rumble_clean_session(sessptr);

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Update thread statistics
         ---------------------------------------------------------------------------------------------------------------
         */

        pthread_mutex_lock(&(master->smtp.mutex));
        foreach((sessionHandle *), s, master->smtp.handles, iter) {
            if (s == sessptr) {
                dvector_delete(&iter);
                x = 1;
                break;
            }
        }

        /*$2
         ---------------------------------------------------------------------------------------------------------------
            Check if we were told to go kill ourself::(
         ---------------------------------------------------------------------------------------------------------------
         */

        if (session._tflags & RUMBLE_THREAD_DIE) {

            /*~~~~~~~~~~~~*/
            pthread_t   *el;
            /*~~~~~~~~~~~~*/

#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
            printf("<smtp::threads>I (%#x) was told to die :(\n", (uintptr_t) pthread_self());
#endif
#ifdef PTW32_CDECL
            pp = (void *) p.p;
#else
            pp = p;
#endif
            foreach((pthread_t *), el, master->smtp.threads, iter)
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

            pthread_mutex_unlock(&master->smtp.mutex);
            pthread_exit(0);
        }

        pthread_mutex_unlock(&master->smtp.mutex);
    }

    pthread_exit(0);
    return (0);
}

/*
 =======================================================================================================================
    Command specific routines
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_mail(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~*/
    ssize_t     rc;
    uint32_t    max,
                size;
    /*~~~~~~~~~~~~~*/

    /* First, check for the right sequence of commands. */
    if (!(session->flags & RUMBLE_SMTP_HAS_HELO)) return (503); /* We need a HELO/EHLO first */
    if ((session->flags & RUMBLE_SMTP_HAS_MAIL)) return (503);  /* And we shouldn't have gotten a MAIL FROM yet */

    /* Try to fetch standard syntax: MAIL FROM: [whatever] <user@domain.tld> */
    session->sender = rumble_parse_mail_address(parameters);
    if (session->sender) {

        /* Fire events scheduled for pre-processing run */
        rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_MAIL);
        if (rc != RUMBLE_RETURN_OKAY) {

            /* Something went wrong, let's clean up and return. */
            rumble_free_address(session->sender);
            return (rc);
        }

        max = rumble_config_int(master, "messagesizelimit");
        size = atoi(rumble_get_dictionary_value(session->sender->flags, "SIZE"));
        if (max != 0 && size != 0 && size > max) {
            rumble_free_address(session->sender);
            return (552);       /* message too big. */
        }

        /* Look for a BATV signature, and if found, confirm that it's valid */
        if (strstr(session->sender->tag, "prvs=")) {

            /*~~~~~~~~~~~~~~~~~~~~~~~*/
            rumbleKeyValuePair  *entry;
            d_iterator          iter;
            /*~~~~~~~~~~~~~~~~~~~~~~~*/

            foreach((rumbleKeyValuePair *), entry, master->_core.batv, iter) {
                if (!strcmp(entry->key, session->sender->tag)) {
                    dvector_delete(&iter);
                    session->flags |= RUMBLE_SMTP_HAS_BATV;
                    free((char *) entry->key);
                    free(entry);
                    break;
                }
            }

            if (!(session->flags & RUMBLE_SMTP_HAS_BATV)) {
                rumble_free_address(session->sender);
                return (530);   /* bounce is invalid or too old. */
            }
        }

        /* Check if it's a supposed (but fake or very very old) bounce */
        if (!strlen(session->sender->domain) && !(session->flags & RUMBLE_SMTP_HAS_BATV)) {
            rumble_free_address(session->sender);
            return (530);       /* bounce is invalid or too old. */
        }

        /* Fire post-processing hooks. */
        rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_MAIL);
        if (rc != RUMBLE_RETURN_OKAY) return (rc);
        session->flags |= RUMBLE_SMTP_HAS_MAIL;
        return (250);
    }

    return (501);   /* Syntax error in MAIL FROM parameter */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_rcpt(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    address     *recipient;
    ssize_t     rc;
    uint32_t    isLocalDomain,
                isLocalUser;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    /* First, check for the right sequence of commands. */
    if (!(session->flags & RUMBLE_SMTP_HAS_MAIL)) return (503);

    /* Allocate stuff and start parsing */
    recipient = rumble_parse_mail_address(parameters);
    if (recipient) {
        dvector_add(session->recipients, recipient);

        /* Fire events scheduled for pre-processing run */
        rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RCPT);
        if (rc != RUMBLE_RETURN_OKAY) {
            dvector_pop(session->recipients);           /* pop the last element from the vector */
            rumble_free_address(recipient);             /* flush the memory */
            return (rc);
        }

        /* Check if recipient is local */
        isLocalDomain = rumble_domain_exists(recipient->domain);
        isLocalUser = isLocalDomain ? rumble_account_exists(session, recipient->user, recipient->domain) : 0;
        if (isLocalUser) {

            /*
             * If everything went fine, set the RCPT flag and return with code 250. ;
             * >>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ;
             * >>>>>>>>>>>>>>>>>>>>>> !!! TODO !!! <<<<<<<<<<<<<<<<<<<<<<< ;
             * Check if user has space in mailbox for this msg! ;
             * >>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ;
             */
            rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_AFTER + RUMBLE_CUE_SMTP_RCPT);
            if (rc != RUMBLE_RETURN_OKAY) {
                dvector_pop(session->recipients);       /* pop the last element from the vector */
                rumble_free_address(recipient);         /* flush the memory */
                return (rc);
            }

            session->flags |= RUMBLE_SMTP_HAS_RCPT;
            return (250);
        }

        /* If rec isn't local, check if client is allowed to relay */
        if (!isLocalDomain) {
            if (session->flags & RUMBLE_SMTP_CAN_RELAY) {

                /* Fire events scheduled for pre-processing run */
                rc = rumble_server_schedule_hooks(master, session,
                                                  RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_AFTER + RUMBLE_CUE_SMTP_RCPT);
                if (rc != RUMBLE_RETURN_OKAY) {
                    dvector_pop(session->recipients);   /* pop the last element from the vector */
                    rumble_free_address(recipient);     /* flush the memory */
                    return (rc);
                }

                session->flags |= RUMBLE_SMTP_HAS_RCPT;
                return (251);
            }

            /* Not local and no relaying allowed, return 530. */
            dvector_pop(session->recipients);
            rumble_free_address(recipient);
            return (530);
        }

        /* Domain is local but user doesn't exist, return 550 */
        dvector_pop(session->recipients);
        rumble_free_address(recipient);
        return (550);
    }

    return (501);   /* Syntax error in RCPT TO parameter */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_helo(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int     rc;
    char    *tmp = (char *) malloc(128);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rc = sscanf(parameters, "%128[%[a-zA-Z0-9%-].%1[a-zA-Z0-9%-]%1[a-zA-Z0-9.%-]", tmp, tmp, tmp);
    if (rc < 3) {
        free(tmp);
        printf("Bad HELO: %s\n", parameters);
        return (504552);    /* simple test for FQDN */
    }

    free(tmp);
    session->flags |= RUMBLE_SMTP_HAS_HELO;
    rsdict(session->dict, "helo", parameters);
    return (250);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_ehlo(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int         rc;
    char        *tmp = (char *) malloc(128);
    char        *el;
    c_iterator  iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rc = sscanf(parameters, "%128[%[a-zA-Z0-9%-].%1[a-zA-Z0-9%-]%1[a-zA-Z0-9.%-]", tmp, tmp, tmp);
    if (rc < 3) {
        free(tmp);
        printf("Bad EHLO: %s\n", parameters);
        return (504552);    /* simple test for FQDN */
    }

    free(tmp);
    session->flags |= RUMBLE_SMTP_HAS_EHLO;
    rumble_comm_send(session, "250-Extended commands follow\r\n");
    cforeach((char *), el, master->smtp.capabilities, iter) {
        rcprintf(session, "250-%s\r\n", el);
    }

    rcsend(session, "250 Done\r\n");
    rsdict(session->dict, "helo", parameters);
    return (RUMBLE_RETURN_IGNORE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_data(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~*/
    char        *fid,
                *filename,
                *log,
                *now,
                *line;
    const char  *sf;
    FILE        *fp;
    address     *el;
    d_iterator  iter;
    /*~~~~~~~~~~~~~~~~~~*/

    /* First, check for the right sequence of commands. */
    if (!(session->flags & RUMBLE_SMTP_HAS_RCPT)) return (503);

    /* Make a unique filename and try to open the storage folder for writing. */
    fid = rumble_create_filename();
    sf = rumble_config_str(master, "storagefolder");
    filename = (char *) calloc(1, strlen(sf) + 26);
    if (!filename) merror();
    sprintf(filename, "%s/%s", sf, fid);
    fp = fopen(filename, "wb");
#ifdef RUMBLE_DEBUG_STORAGE
    printf("Writing to file %s...\n", filename);
#endif
    free(filename);
    if (!fp)
    {
#ifdef RUMBLE_DEBUG_STORAGE
        perror("Couldn't open file for writing");
#endif
        free(fid);
        return (451);   /* Couldn't open file for writing :/ */
    }

    log = (char *) calloc(1, 1024);
    if (!log) merror();
    now = rumble_mtime();
    sprintf(log, "Received: from %s <%s> by %s (rumble) with ESMTPA id <%s>; %s\r\n", rumble_get_dictionary_value(session->dict, "helo"),
            session->client->addr, rumble_config_str(master, "servername"), fid, now);
    free(now);
    fwrite(log, strlen(log), 1, fp);
    rumble_comm_send(session, rumble_smtp_reply_code(354));

    /* Save the message */
    while (1) {
        line = rumble_comm_read(session);

        /* Check for broken connection */
        if (!line) {
            fclose(fp);
            free(fid);
            return (RUMBLE_RETURN_FAILURE);
        }

        if (!strcmp(line, ".\r\n")) break;  /* We're done here. */
        if (fwrite(line, strlen(line), 1, fp) != 1) {

            /* Writing failed? */
            fclose(fp);
            free(fid);
            return (452);
        }
    }

    fclose(fp);
    foreach((address *), el, session->recipients, iter) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        void    *state = rumble_database_prepare(master->_core.db, "INSERT INTO queue (fid, sender, recipient, flags) VALUES (%s,%s,%s,%s)",
                                                 fid, session->sender->raw, el->raw, session->sender->_flags);
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        rumble_database_run(state);
        rumble_database_cleanup(state);
    }

    free(fid);
    return (250);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_rset(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~*/
    ssize_t rc;
    /*~~~~~~~*/

    /* Fire events scheduled for pre-processing run */
    rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RSET);
    if (rc != RUMBLE_RETURN_OKAY) return (rc);

    /* Reset the session handle */
    session->flags = 0;
    rumble_clean_session(session);

    /* Fire post-processing hooks. */
    rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RSET);
    if (rc != RUMBLE_RETURN_OKAY) return (rc);
    return (250);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_vrfy(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    ssize_t rc;
    char    *user = (char *) calloc(1, 128);
    char    *domain = (char *) calloc(1, 128);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (sscanf(parameters, "%128[^@\"]@%128[^\"]", user, domain)) {

        /* Fire events scheduled for pre-processing run */
        rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_VRFY);
        if (rc != RUMBLE_RETURN_OKAY) return (rc);

        /* Check if account exists */
        return (rumble_account_exists(session, user, domain) ? 250 : 550);
    }

    return (501);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_noop(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~*/
    ssize_t rc;
    /*~~~~~~~*/

    /* Fire events scheduled for pre-processing run */
    rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_NOOP);
    if (rc != RUMBLE_RETURN_OKAY) return (rc);

    /*
     * Do...nothing ;
     * Fire post-processing hooks.
     */
    rc = rumble_server_schedule_hooks(master, session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_NOOP);
    if (rc != RUMBLE_RETURN_OKAY) return (rc);
    return (250);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t rumble_server_smtp_auth(masterHandle *master, sessionHandle *session, const char *parameters, const char *extra_data) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            method[31],
                    digest[1025],
                    *buffer,
                    *user,
                    *pass,
                    *line;
    rumble_mailbox  *OK;
    address         *addr;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

    memset(method, 0, 31);
    memset(digest, 0, 1025);
    printf("Got: %s\n", parameters);
    sscanf(parameters, "%30s %1024s", method, digest);
    rumble_string_lower(method);
    pass = "";
    addr = 0;
    OK = 0;

    /* LOGIN method */
    if (!strcmp(method, "login")) {

        /* Username */
        rcsend(session, "334 VXNlcm5hbWU6\r\n");
        line = rcread(session);
        sscanf(line, "%s", digest);
        user = rumble_decode_base64(digest);

        /* Password */
        rcsend(session, "334 UGFzc3dvcmQ6\r\n");
        line = rcread(session);
        sscanf(line, "%s", digest);
        pass = rumble_decode_base64(digest);
        addr = rumble_parse_mail_address(user);
        if (addr) OK = rumble_account_data_auth(0, addr->user, addr->domain, pass);
        free(user);
        strcpy(digest, pass);
        free(pass);
        pass = digest;
    }

    /* PLAIN method */
    if (!strcmp(method, "plain")) {
        buffer = rumble_decode_base64(digest);
        user = buffer + 1;
        pass = buffer + 2 + strlen(user);
        addr = rumble_parse_mail_address(user);
        if (addr) OK = rumble_account_data_auth(0, addr->user, addr->domain, pass);
        free(buffer);
    }

    if (OK) {
        if (rumble_account_data_auth(0, addr->user, addr->domain, pass)) {
            session->flags |= RUMBLE_SMTP_CAN_RELAY;
            rumble_free_account(OK);
            return (250);
        } else {
            session->flags -= (session->flags & RUMBLE_SMTP_CAN_RELAY);
            return (530);
        }
    }

    return (501);
}
