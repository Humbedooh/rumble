/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "sqlite3.h"
#include "servers.h"
#include "private.h"
#include "database.h"
#include "comm.h"
#include <sys/stat.h>
#ifdef __GNUC__
#   include <dirent.h>
#endif
mqueue  *current = 0;
dvector *badmx;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void get_smtp_response(sessionHandle *session, rumble_sendmail_response *res) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    char            *line,
                    *flag;
    unsigned char   b = '-';
    /*~~~~~~~~~~~~~~~~~~~~*/

    res->replyCode = 500;
    flag = (char *) calloc(1, 200);
    if (!flag) merror();
    if (res) {
        while (b == '-') {
            line = rcread(session);
            res->replyCode = 500;
            if (!line) break;

            /*
             * printf("MTA: %s\n", line);
             */
            memset(res->replyMessage, 0, 1000);
            if (sscanf(line, "%3u%c%200c", &res->replyCode, &b, res->replyMessage) < 2) {
                res->replyCode = 500;
                break;
            }

            memset(flag, 0, 200);
            if (sscanf(line, "%*3u%*1[ %-]%20[A-Z0-9]", flag)) {
                if (strlen(flag) > 2) {
                    rsdict(res->flags, flag, flag);
                }
            }

            free(line);
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_sendmail_response *rumble_send_email(
    masterHandle    *master,
    const char      *mailserver,
    const char      *filename,
    address         *sender,
    address         *recipient) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_sendmail_response    *res;
    sessionHandle               s;
    clientHandle                c;
    FILE                        *fp;
    size_t                      fsize,
                                chunk;
    char                        buffer[2048];
    const char                  *me;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    s.client = &c;
    s._svc = 0;
    res = (rumble_sendmail_response *) malloc(sizeof(rumble_sendmail_response));
    if (!res) merror();
    res->flags = dvector_init();
    res->replyCode = 500;
    res->replyMessage = (char *) calloc(1, 1024);
    res->replyServer = (char *) calloc(1, strlen(mailserver) + 1);
    if (!res->replyServer || !res->replyMessage || !res->flags) merror();
    strncpy(res->replyServer, mailserver, strlen(mailserver));
    sprintf(res->replyMessage, "Server could not be reached.");
    fp = fopen(filename, "rb");
    if (!fp) {
        perror("Couldn't open file!");
        return (res);
    }

    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    rewind(fp);
    res->replyCode = 250;
    printf("connecting to %s...\n", mailserver);
    c.tls = 0;
    c.socket = comm_open(master, mailserver, 25);
    c.recv = 0;
    c.send = 0;
    me = rumble_get_dictionary_value(master->_core.conf, "servername");
    FD_ZERO(&c.fd);
    FD_SET(c.socket, &c.fd);

    /* Append BATV (unless we already have BATV or VERP) */
    if (!strlen(sender->tag)) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        rumbleKeyValuePair  *el;
        char                *batv = rumble_create_filename();
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        sprintf(sender->tag, "prvs=%s", batv);
        el = (rumbleKeyValuePair *) malloc(sizeof(rumbleKeyValuePair));
        if (!el) merror();
        el->key = batv;
        el->value = (char *) time(0);
        dvector_add(master->_core.batv, el);
    }

    while (c.socket) {
        get_smtp_response(&s, res);
        if (res->replyCode >= 300) break;

        /* Try EHLO first */
        rcprintf(&s, "EHLO %s\r\n", me);
        get_smtp_response(&s, res);
        if (res->replyCode >= 300) {

            /* Or...try HELO */
            rcprintf(&s, "HELO %s\r\n", me);
            get_smtp_response(&s, res);
            if (res->replyCode >= 300) break;
        }

        /* Do a MAIL FROM */
        if (rhdict(res->flags, "SIZE")) {
            rcprintf(&s, "MAIL FROM: <%s=%s@%s> SIZE=%u\r\n", sender->tag, sender->user, sender->domain, fsize);
        } else {
            rcprintf(&s, "MAIL FROM: <%s=%s@%s>\r\n", sender->tag, sender->user, sender->domain);
        }

        get_smtp_response(&s, res);
        if (res->replyCode >= 300) break;

        /* Do an RCPT TO */
        rcprintf(&s, "RCPT TO: <%s@%s>\r\n", recipient->user, recipient->domain);
        get_smtp_response(&s, res);
        if (res->replyCode >= 300) break;

        /* Do a DATA */
        rcprintf(&s, "DATA\r\n", sender);
        get_smtp_response(&s, res);
        if (res->replyCode >= 400) break;
        while (!feof(fp)) {
            memset(buffer, 0, 2000);
            chunk = fread(buffer, 1, 2000, fp);
            send(c.socket, buffer, (int) chunk, 0);
        }

        rcsend(&s, ".\r\n");
        get_smtp_response(&s, res);
        break;
    }

    fclose(fp);
    rcprintf(&s, "QUIT\r\n", sender);
    if (c.socket) disconnect(c.socket);
    return (res);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_prune_storage(const char *folder) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    struct stat     fileinfo;
    time_t          now;
#ifdef __GNUC__
    DIR             *dir = 0;
    struct dirent   *dirp;
    char            filename[512];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    now = time(0);
    dir = opendir(folder);
    if (dir != 0) {
        while ((dirp = readdir(dir))) {
            if (dirp->d_name[0] == '.' || strstr(dirp->d_name, ".msg")) continue;
            sprintf(filename, "%s/%s", folder, dirp->d_name);
            if (stat(filename, &fileinfo) == -1) continue;
            if ((now - fileinfo.st_atime) > 43200) {
                unlink(filename);
            }
        }
    }

#else
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    LPWIN32_FIND_DATAA  fd;
    HANDLE              fh;
    char                path[MAX_PATH],
                        filename[MAX_PATH + 64];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    fd = (LPWIN32_FIND_DATAA) malloc(sizeof(WIN32_FIND_DATAA));
    sprintf(path, "%s/*", folder);
    now = time(0);
    fh = FindFirstFileA((LPCSTR) path, fd);
    if (fh != INVALID_HANDLE_VALUE) {
        do
        {
            if (!strchr(fd->cFileName, '.')) {
                sprintf(filename, "%s/%s", folder, fd->cFileName);
                if (stat(filename, &fileinfo) == -1) continue;
                if ((now - fileinfo.st_atime) > 43200) {
                    unlink(filename);
                }
            }
        } while (FindNextFileA(fh, fd));
    }

    FindClose(fh);
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *rumble_sanitation_process(void *m) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc = (rumbleService *) m;
    const char      *mainpath,
                    *localpath;
    masterHandle    *master = svc->master;
    d_iterator      iter;
    rumble_domain   *domain;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mainpath = rumble_config_str(master, "storagefolder");
    localpath = 0;
    while (1) {

        /* Check the main storage folder. */
        rumble_prune_storage(mainpath);

        /* Check for invididually set storage folders. */
        rumble_rw_start_read(master->domains.rrw);
        foreach((rumble_domain *), domain, master->domains.list, iter) {
            localpath = domain->path;
            if (localpath and strlen(localpath) and strcmp(localpath, mainpath)) {
                rumble_prune_storage(localpath);
            }
        }

        rumble_rw_stop_read(master->domains.rrw);
        sleep(14400);
    }

    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *rumble_worker_process(void *m) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc = (rumbleService *) m;
    mqueue          *item;
    char            *tmp;
    rumble_mailbox  *user;
    ssize_t         rc;
    masterHandle    *master = svc->master;
    sessionHandle   *sess = (sessionHandle *) malloc(sizeof(sessionHandle)),
                    *s;
    clientHandle    c;
    d_iterator      diter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sess->client = &c;
    if (!sess) merror();
    sess->_master = (masterHandle *) svc->master;
    tmp = (char *) calloc(1, 256);
    while (1) {
        pthread_mutex_lock(&svc->mutex);
        pthread_cond_wait(&svc->cond, &svc->mutex);
        dvector_add(svc->handles, (void *) sess);
        svc->traffic.sessions++;

        /* Make a private copy of the mail struct address and reset the global one */
        item = current;
        current = 0;
        pthread_mutex_unlock(&svc->mutex);
        if (!item) continue;

        /* Check for rampant loops */
        item->loops++;
        if (item->loops > 5) {
            if (item->recipient) rumble_free_address(item->recipient);
            if (item->sender) rumble_free_address(item->recipient);
            if (item->fid) free((char *) item->fid);
            if (item->flags) free((char *) item->flags);
            free(item);
            printf("Mail is looping, ignoring it!\n");
            continue;
        }

        /* Local delivery? */
        if (rumble_domain_exists(item->recipient->domain)) {
            rumble_debug("mailman", "Have mail for %s (local domain), looking for user %s@%s", item->recipient->domain, item->recipient->user, item->recipient->domain);
            user = rumble_account_data(0, item->recipient->user, item->recipient->domain);
            if (user) {

                /*~~~~~~~~~~*/
                size_t  fsize;
                /*~~~~~~~~~~*/

                item->account = user;

                /* pre-delivery parsing (virus, spam, that sort of stuff) */
                rc = rumble_server_schedule_hooks(master, (sessionHandle *) item, RUMBLE_HOOK_PARSER);  /* hack, hack, hack */
                if (rc == RUMBLE_RETURN_OKAY) {
                    if (user->type & RUMBLE_MTYPE_MBOX) {

                        /*~~~~~~~~~~~~~~~~~~~*/
                        /* mail box */
                        char        *ofilename,
                                    *nfilename;
                        const char  *path;
                        /*~~~~~~~~~~~~~~~~~~~*/

                        rumble_debug("mailman", "Delivering message %s to mailbox %s @ %s...", item->fid, user->user, user->domain->name);

                        /* Start by making a copy of the letter */
                        fsize = rumble_copy_mail(master, item->fid, user->user, user->domain->name, (char **) &item->fid);
                        if (!item->fid || !fsize) {
                            fprintf(stderr, "<smtp::worker> Bad mail file, aborting\n");
                            continue;
                        }

                        /* move file to user's inbox */
                        path = rumble_config_str(master, "storagefolder");
                        ofilename = (char *) calloc(1, strlen(path) + 26);
                        nfilename = (char *) calloc(1, strlen(path) + 26);
                        sprintf(ofilename, "%s/%s", path, item->fid);
                        sprintf(nfilename, "%s/%s.msg", path, item->fid);
#ifdef RUMBLE_DEBUG_STORAGE
                        rumble_debug("mailman", "Moving %s to %s", ofilename, nfilename);
#endif
                        if (rename(ofilename, nfilename)) {
                            perror("Couldn't move file");
                        }

                        free(ofilename);
                        free(nfilename);
                        radb_run_inject(master->_core.mail, "INSERT INTO mbox (uid, fid, size, flags) VALUES (%u, %s, %u,0)",
                                        item->account->uid, item->fid, fsize);

                        /* done here! */
                    }

                    if (user->type & RUMBLE_MTYPE_ALIAS) {

                        /* mail alias */
                        if (strlen(user->arg)) {

                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                            char    *pch = strtok(user->arg, " ");
                            char    *email = (char *) calloc(1, 256);
                            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                            while (pch != NULL) {
                                memset(email, 0, 128);
                                if (strlen(pch) >= 3) {

                                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                                    char    *loops = (char *) calloc(1, 4);
                                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                                    sprintf(loops, "%u", item->loops);
                                    if (sscanf(pch, "%256c", email)) {
                                        rumble_string_lower(email);
                                        radb_run_inject(master->_core.mail,
                                                        "INSERT INTO queue (loops, fid, sender, recipient, flags) VALUES (%s,%s,%s,%s,%s)",
                                                        loops, item->fid, item->sender->raw, email, item->flags);
                                    }
                                }

                                pch = strtok(NULL, " ");
                            }

                            free(email);
                        }

                        /* done here! */
                    }

                    if (user->type & RUMBLE_MTYPE_MOD) {

                        /* feed to module */
                        printf("<worker> Feeding mail to module %s\n", user->arg);
                        rumble_server_schedule_hooks(master, (sessionHandle *) item, RUMBLE_HOOK_FEED); /* hack, hack, hack */
                        /* done here! */
                    }

                    if (user->type & RUMBLE_MTYPE_FEED) {

                        /*
                         * feed to program or url ;
                         * done here!
                         */
                    }
                }

                rumble_free_account(user);
            } else {
                printf("I couldn't find %s :(\n", item->recipient->raw);
            }
        } /* Foreign delivery? */ else {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            dvector                     *mx;
            mxRecord                    *mxr;
            char                        *filename;
            const char                  *statement;
            uint32_t                    delivered = 500;
            rumble_sendmail_response    *res;
            d_iterator                  iter;
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

           rumble_debug("mailman", "mail %s: %s@%s is a foreign user, finding host.", item->fid, item->recipient->user, item->recipient->domain);
            mx = comm_mxLookup(item->recipient->domain);
            if (!mx or!mx->size) {
                rumble_debug("mailman", "Couldn't look up domain %s, faking a SMTP 450 error.", item->recipient->domain);
                delivered = 450;
            } else if (mx->size) {
                filename = (char *) calloc(1, 256);
                if (!filename) merror();
                sprintf(filename, "%s/%s", rrdict(master->_core.conf, "storagefolder"), item->fid);
                foreach((mxRecord *), mxr, mx, iter) {
                    if (rhdict(badmx, mxr->host)) continue; /* ignore bogus MX records */
                    rumble_debug("mailman", "Trying %s (%u)...\n", mxr->host, mxr->preference);

                    /* Anything below 300 would be good here :> */
                    res = rumble_send_email(master, mxr->host, filename, item->sender, item->recipient);

                    /* get the best result from all servers we've tried */
                    delivered = (res->replyCode < delivered) ? res->replyCode : delivered;
                    rumble_flush_dictionary(res->flags);
                    free(res->replyMessage);
                    free(res);
                    if (delivered <= 299) break;            /* yay! */
                    rumble_debug("mailman", "MTA <%s> returned code %d (%s)", res->replyServer, delivered, res->replyMessage);
                }

                free(filename);
            }

            if (delivered >= 500) {

                /* critical failure, giving up. */
                rumble_debug("mailman", "Critical failure, giving up for now.",0);
            } else if (delivered >= 400) {

                /* temp failure, push mail back into queue (schedule next try in 30 minutes). */
                rumble_debug("mailman", "MTA reported temporary error(%u), queuing mail for later", delivered);
                sprintf(tmp, "<%s=%s@%s>", item->sender->tag, item->sender->user, item->sender->domain);
                statement = "INSERT INTO queue (time, loops, fid, sender, recipient, flags) VALUES (strftime('%%s', 'now', '+10 minutes'),%u,%s,%s,%s,%s)";
                if (master->_core.mail->dbType == RADB_MYSQL) {
                    statement = "INSERT INTO queue (time, loops, fid, sender, recipient, flags) VALUES (NOW( ) + INTERVAL 10 MINUTE,%u,%s,%s,%s,%s)";
                }

                radb_run_inject(master->_core.mail, statement, item->loops, item->fid, tmp, item->recipient->raw, item->flags);
                rumble_debug("mailman", "Mail %s queued", item->fid);
                memset(tmp, 0, 256);
            } else {
                rumble_debug("mailman", "Mail %s delivered.", item->fid);
            }

            if (mx) comm_mxFree(mx);    /* Clean up DNS records. */
            /* All done! */
        }

        if (item->recipient) rumble_free_address(item->recipient);
        if (item->sender) rumble_free_address(item->recipient);
        if (item->fid) free((char *) item->fid);
        if (item->flags) free((char *) item->flags);
        item->account = 0;
        free(item);
        foreach((sessionHandle *), s, svc->handles, diter) {
            if (s == sess) {
                dvector_delete(&diter);
                break;
            }
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *rumble_worker_init(void *T) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleThread    *thread = (rumbleThread *) T;
    rumbleService   *svc = (rumbleService *) thread->svc;
    masterHandle    *master = (masterHandle *) svc->master;
    int             x;
    const char      *ignmx;
    const char      *statement = "SELECT time, loops, fid, sender, recipient, flags, id FROM queue WHERE time <= strftime('%%s','now') LIMIT 4";
    radbObject      *dbo;
    radbResult      *result;
    pthread_attr_t  attr;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (master->_core.mail->dbType == RADB_MYSQL) {
        statement = "SELECT time, loops, fid, sender, recipient, flags, id FROM queue WHERE time <= NOW() LIMIT 4";
    }

    ignmx = rrdict(master->_core.conf, "ignoremx");
    badmx = dvector_init();
    if (strlen(ignmx)) rumble_scan_words(badmx, ignmx);
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128 * 1024);   /* let's see if 512kb is enough
                                                     * ;
                                                     * > */

    /* MTA workers */
    for (x = 0; x < 20; x++) {
        thread = (rumbleThread *) malloc(sizeof(rumbleThread));
        cvector_add(svc->threads, thread);
        thread->status = 1;
        pthread_create(&thread->thread, &attr, rumble_worker_process, svc);
    }

    /* Trash collector */
    thread = (rumbleThread *) malloc(sizeof(rumbleThread));
    cvector_add(svc->threads, thread);
    thread->status = 1;
    pthread_create(&thread->thread, &attr, rumble_sanitation_process, svc);
    dbo = radb_prepare(master->_core.mail, statement);
    if (!dbo) printf("Something went wrong with this: %s\n", statement);
    while (1) {
        result = radb_fetch_row(dbo);
        if (result) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            uint32_t    mid;
            mqueue      *item = (mqueue *) calloc(1, sizeof(mqueue));
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            if (item) {
                item->mType = 0;

                /* delivery time */
                item->date = result->column[0].data.uint32;

                /* loops */
                item->loops = result->column[1].data.uint32;

                /* fid */
                item->fid = strclone(result->column[2].data.string);

                /* sender */
                item->sender = rumble_parse_mail_address(result->column[3].data.string);

                /* recipient */
                item->recipient = rumble_parse_mail_address(result->column[4].data.string);

                /* flags */
                item->flags = strclone(result->column[5].data.string);
                mid = result->column[6].data.uint32;
                radb_run_inject(master->_core.mail, "DELETE FROM queue WHERE id = %u", mid);
                fflush(stdout);
                if (!item->sender or!item->recipient) {
                    printf("BAD: Sender or Recipient is invalid, discarding mail.\n");
                    if (item->recipient) rumble_free_address(item->recipient);
                    if (item->sender) rumble_free_address(item->recipient);
                    if (item->fid) free((char *) item->fid);
                    if (item->flags) free((char *) item->flags);
                    item->account = 0;
                    free(item);
                } else {
                    pthread_mutex_lock(&svc->mutex);
                    current = item;
                    pthread_cond_signal(&svc->cond);
                    pthread_mutex_unlock(&svc->mutex);
                }
            }
        } else {
            radb_cleanup(dbo);
            sleep(1);   /* sleep for 1 seconds if there's nothing to do right now. */
            dbo = radb_prepare(master->_core.mail, statement);
        }
    }

    return (0);
}
