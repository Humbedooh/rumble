/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "sqlite3.h"
#include "servers.h"
#include "private.h"
#include "database.h"
#include "mailman.h"
#include "comm.h"
#include <sys/stat.h>
#ifdef __GNUC__
#   include <dirent.h>
#endif
mqueue              *current = 0;
dvector             *badmx;
static const char   *svcs[] = { "imap4", "pop3", "smtp", "mailman", 0 };

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

        closedir(dir);
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
    rumble_mailbox  *user;
    ssize_t         rc;
    masterHandle    *master = svc->master;
    sessionHandle   *sess = (sessionHandle *) malloc(sizeof(sessionHandle)),
                    *s;
    clientHandle    c;
    d_iterator      diter;
    int maxAttempts = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    maxAttempts = atoi(rrdict(master->_core.conf, "deliveryattempts"));
    maxAttempts = maxAttempts ? maxAttempts : 5;
    sess->client = &c;
    sess->_svc = svc;
    if (!sess) merror();
    sess->_master = (masterHandle *) svc->master;
    while (1) {
        pthread_mutex_lock(&svc->mutex);
        pthread_cond_wait(&svc->cond, &svc->mutex);
        dvector_add(svc->handles, (void *) sess);
        svc->traffic.sessions++;

        /* Move the item struct to a private address and free up the global one */
        item = current;
        current = 0;
        pthread_mutex_unlock(&svc->mutex);
        if (!item) continue;

        /* Check for rampant loops */
        item->loops++;
        if (item->loops > maxAttempts) {
            rumble_debug(NULL, "mailman", "Message %s is looping, dumping it!\n", item->fid);
            if (strcmp(item->sender->user, "mailman") || strcmp(item->sender->domain, "localhost")) {
                smtp_deliver_failure(master, item->sender->raw, item->recipient->raw, "Reason: Message seems to be looping.");
            }
            if (item->recipient) rumble_free_address(item->recipient);
            if (item->sender) rumble_free_address(item->sender);
            if (item->fid) free((char *) item->fid);
            if (item->flags) free((char *) item->flags);
            free(item);
            continue;
        }

        /* Local delivery? */
        if (rumble_domain_exists(item->recipient->domain)) {
            rumble_debug(NULL, "mailman", "Have mail for %s (local domain), looking for user %s@%s", item->recipient->domain,
                         item->recipient->user, item->recipient->domain);
            user = rumble_account_data(0, item->recipient->user, item->recipient->domain);
            if (user) {

                /*~~~~~~~~~~~~~~~~~*/
                int     knowType = 0;
                /*~~~~~~~~~~~~~~~~~*/

                item->account = user;

                /* pre-delivery parsing (virus, spam, that sort of stuff) */
                rc = rumble_server_schedule_hooks(master, (sessionHandle *) item, RUMBLE_HOOK_PARSER);  /* hack, hack, hack */
                if (rc == RUMBLE_RETURN_OKAY) {
                    if (user->type & RUMBLE_MTYPE_MBOX) knowType = smtp_deliver_mbox(user, item, master);
                    if (user->type & RUMBLE_MTYPE_ALIAS) knowType = smtp_deliver_alias(user, item, master);
                    if (user->type & RUMBLE_MTYPE_FEED) knowType = smtp_deliver_feed(user, item, master);
                    if (user->type & RUMBLE_MTYPE_RELAY) knowType = smtp_deliver_foreign(item, master, user->arg);
                    if (user->type & RUMBLE_MTYPE_MOD)
                        knowType = rumble_server_schedule_hooks(master, (sessionHandle *) item, RUMBLE_HOOK_FEED);
                }

                if (knowType == 0) {
                    rumble_debug(master, "mailman", "Account <%s@%s> has unknown mailbox type, ignoring mail :(\n", user->user, user->domain->name);
                }

                rumble_free_account(user);
            } else {
                printf("I couldn't find %s :(\n", item->recipient->raw);
            }
        } /* Foreign delivery? */ else smtp_deliver_foreign(item, master, item->recipient->domain);

        /*$4
         ***************************************************************************************************************
            Local cleanup
         ***************************************************************************************************************
         */

        if (item->recipient) rumble_free_address(item->recipient);
        if (item->sender) rumble_free_address(item->sender);
        if (item->fid) free((char *) item->fid);
        if (item->flags) free((char *) item->flags);
        item->account = 0;
        free(item);

        /*$4
         ***************************************************************************************************************
            Signal we're free for another job
         ***************************************************************************************************************
         */

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
    const char      *statement = "SELECT time, loops, fid, sender, recipient, flags, id FROM queue WHERE time <= strftime('%%s','now') LIMIT 4";
    radbObject      *dbo;
    radbResult      *result;
    pthread_attr_t  attr;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sleep(3);   /* wait for the others :> */
    if (master->_core.mail->dbType == RADB_MYSQL) {
        statement = "SELECT time, loops, fid, sender, recipient, flags, id FROM queue WHERE time <= NOW() LIMIT 4";
    }

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

            /*~~~~~~~~~~~~~~~~~~~~~~*/
            /* Update traffic stats while we're doing nothing */
            rumbleService   *xsvc = 0;
            int             K = 0;
            /*~~~~~~~~~~~~~~~~~~~~~~*/

            for (K = 0; K < 4; K++) {
                xsvc = comm_serviceHandle(svcs[K]);
                if (xsvc) comm_addEntry(xsvc, 0, 100);
            }

            radb_cleanup(dbo);
            sleep(3);   /* sleep for 3 seconds if there's nothing to do right now. */
            dbo = radb_prepare(master->_core.mail, statement);
        }
    }

    radb_cleanup(dbo);
    return (0);
}
