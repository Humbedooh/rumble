/*$I0 */
#include "rumble.h"
#include "sqlite3.h"
#include "servers.h"
#include "private.h"
#include "database.h"
#include "comm.h"
#include "mailman.h"
#include <sys/stat.h>

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int smtp_deliver_feed(rumble_mailbox *user, mqueue *item, masterHandle *master) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char        tempfile[L_tmpnam];
    char        buffer[2001];
    char        xarg[2001];
    char        xuser[129];
    char        xdomain[129];
    char        *filename;
    const char  *path;
    FILE        *fp;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    path = rumble_config_str(master, "storagefolder");
    filename = (char *) calloc(1, strlen(path) + 26);
    sprintf(filename, "%s/%s", path, item->fid);
    rumble_debug(master, "mailman", "Feeding email to program <%s>", user->arg);
    memset(tempfile, 0, L_tmpnam);
    tmpnam(tempfile);
#ifdef RUMBLE_MSC
    sprintf(buffer, "< \"%s\" > \"%s\"", filename, tempfile);
    printf("Executing: %s\n", buffer);
    execl(user->arg, buffer, 0);

    /*
     * ShellExecuteA( NULL, "open", sa_exec,buffer, "",SW_SHOW);
     */
#else
    sprintf(buffer, "%s < %s > %s", user->arg, filename, tempfile);
    printf("Executing: %s\n", buffer);
    system(buffer);
#endif
    free(filename);
    fp = fopen(tempfile, "rb");
    if (fp) {

        /*~~~~~~*/
        size_t  i;
        /*~~~~~~*/

        if (!fgets(buffer, 2000, fp)) memset(buffer, 0, 2000);
        while (strlen(buffer) > 2) {
            i = strlen(buffer);
            memset(xarg, 0, 2001);
            memset(xuser, 0, 129);
            memset(xdomain, 0, 129);
            if (buffer[i - 1] == '\n') buffer[i - 1] = 0;
            if (buffer[i - 2] == '\r') buffer[i - 2] = 0;
            if (sscanf(buffer, "R-FORWARD %256c", xarg)) {
                rumble_debug(master, "mailman", "Forwarding letter to %s\n", xarg);
                radb_run_inject(master->_core.mail,
                                "INSERT INTO queue (id,loops, fid, sender, recipient, flags) VALUES (NULL,%u,%s,%s,%s,%s)", 1, item->fid,
                                item->sender->raw, xarg, item->flags);
            }

            if (sscanf(buffer, "R-REPLY %256c", xarg)) {

                /*~~~~~~~~~~~~~~~*/
                char    *fid = 0;
                size_t  length = 0;
                /*~~~~~~~~~~~~~~~*/

                rumble_debug(master, "mailman", "Replying with message file <%s>\n", xarg);
                length = rumble_mail_from_file(master, xarg, &fid);
                if (length) {
                    radb_run_inject(master->_core.mail,
                                    "INSERT INTO queue (id,loops, fid, sender, recipient, flags) VALUES (NULL,%u,%s,%s,%s,%s)", 1, fid,
                                    item->recipient->raw, item->sender->raw, item->flags);
                }
            }

            if (sscanf(buffer, "R-SEND <%128[^@ ]@%128[^>]> %256c", xuser, xdomain, xarg) == 3) {

                /*~~~~~~~~~~~~~~~~~~~*/
                char    *fid = 0;
                char    recipient[260];
                size_t  length = 0;
                /*~~~~~~~~~~~~~~~~~~~*/

                rumble_debug(master, "mailman", "Sending message <%s> to <%s@%s>\n", xarg, xuser, xdomain);
                sprintf(recipient, "<%s@%s>", xuser, xdomain);
                length = rumble_mail_from_file(master, xarg, &fid);
                if (length) {
                    radb_run_inject(master->_core.mail,
                                    "INSERT INTO queue (id,loops, fid, sender, recipient, flags) VALUES (NULL,%u,%s,%s,%s,%s)", 1, fid,
                                    item->recipient->raw, recipient, item->flags);
                }
            }

            if (sscanf(buffer, "R-DELETE %256c", xarg)) {
                rumble_debug(master, "mailman", "Deleting file <%s>\n", xarg);
                unlink(xarg);
            }

            if (!fgets(buffer, 2000, fp)) break;
        }

        fclose(fp);
    }

    unlink(tempfile);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int smtp_deliver_alias(rumble_mailbox *user, mqueue *item, masterHandle *master) {
    rumble_debug(master, "mailman", "%s@%s is an alias, looking up arguments", user->user, user->domain->name);
    if (strlen(user->arg)) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        char    *pch = strtok(user->arg, " ,;");
        char    *email = (char *) calloc(1, 256);
        char    xemail[256];
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
                    memset(xemail, 0, 256);
#ifdef RUMBLE_MSC
                    _snprintf(xemail, 255, "<%s>", email);
#else
                    snprintf(xemail, 255, "<%s>", email);
#endif
                    rumble_debug(master, "mailman", "Delivering message %s to alias %s...", item->fid, xemail);
                    radb_run_inject(master->_core.mail,
                                    "INSERT INTO queue (id,loops, fid, sender, recipient, flags) VALUES (NULL,%s,%s,%s,%s,%s)", loops,
                                    item->fid, item->sender->raw, xemail, item->flags);
                }
            }

            pch = strtok(NULL, " ,;");
        }

        free(email);
    } else {
        rumble_debug(master, "mailman", "No arguments supplied for alias account!");
    }

    return (1);

    /* done here! */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int smtp_deliver_mbox(rumble_mailbox *user, mqueue *item, masterHandle *master) {

    /*~~~~~~~~~~~~~~~~~~~*/
    /* mail box */
    char        *ofilename,
                *nfilename;
    const char  *defaultPath, *domainStoragePath;
    size_t      fsize;
    /*~~~~~~~~~~~~~~~~~~~*/

    rumble_debug(master, "mailman", "Delivering message %s to mailbox %s @ %s...", item->fid, user->user, user->domain->name);

    /* Start by making a copy of the letter */
    fsize = rumble_copy_mail(master, item->fid, user->user, user->domain->name, (char **) &item->fid);
    if (!item->fid || !fsize) {
        if (item->fid) free((void *) item->fid);
        rumble_debug(master, "mailman", "message %s could not be read, aborting", item->fid);
        return 1;
    }

    /* move file to user's inbox */
    defaultPath = rumble_config_str(master, "storagefolder");
    domainStoragePath = strlen(user->domain->path) ? user->domain->path : defaultPath;
    
    ofilename = (char *) calloc(1, strlen(defaultPath) + 26);
    nfilename = (char *) calloc(1, strlen(domainStoragePath) + 26);
    sprintf(ofilename, "%s/%s", defaultPath, item->fid);
    sprintf(nfilename, "%s/%s.msg", domainStoragePath, item->fid);
#ifdef RUMBLE_DEBUG_STORAGE
    rumble_debug(master, "mailman", "Moving %s to %s", ofilename, nfilename);
#endif
    if (rename(ofilename, nfilename)) {
        perror("Couldn't move file");
    }

    free(ofilename);
    free(nfilename);
    radb_run_inject(master->_core.mail, "INSERT INTO mbox (id,uid, fid, size, flags) VALUES (NULL,%u, %s, %u,0)", item->account->uid,
                    item->fid, fsize);

    /* done here! */
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int smtp_deliver_foreign(mqueue *item, masterHandle *master, const char *host) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    dvector                     *mx;
    mxRecord                    *mxr;
    char                        *filename;
    char                        tmp[512];
    char                        serverReply[2048];
    char                        statement[1024];
    uint32_t                    delivered = 500;
    rumble_sendmail_response    *res;
    d_iterator                  iter;
    const char*ignmx;
    dvector* badmx;
    int maxAttempts = 5;
    int retryInterval = 360;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    maxAttempts = atoi(rrdict(master->_core.conf, "deliveryattempts"));
    maxAttempts = maxAttempts ? maxAttempts : 5;
    
    retryInterval = atoi(rrdict(master->_core.conf, "retryinterval"));
    retryInterval = retryInterval ? retryInterval : 360;
    
    ignmx = rrdict(master->_core.conf, "ignoremx");
    badmx = dvector_init();
    if (strlen(ignmx)) rumble_scan_words(badmx, ignmx);
    
    rumble_debug(master, "mailman", "mail %s: %s@%s is a foreign user, finding host <%s>.", item->fid, item->recipient->user,
                 item->recipient->domain, host);
    mx = comm_mxLookup(host);
    if (!mx or!mx->size) {
        rumble_debug(master, "mailman", "Couldn't look up domain %s, faking a SMTP 450 error.", host);
        delivered = 450;
        sprintf(serverReply, "Reason: Unable to resolve hostname '%s'", host);
    } else if (mx->size) {
        filename = (char *) calloc(1, 256);
        if (!filename) merror();
        sprintf(filename, "%s/%s", rrdict(master->_core.conf, "storagefolder"), item->fid);
        foreach((mxRecord *), mxr, mx, iter) {
            if (rhdict(badmx, mxr->host)) continue; /* ignore bogus MX records */
            rumble_debug(master, "mailman", "Trying %s (%u)...\n", mxr->host, mxr->preference);

            /* Anything below 300 would be good here :> */
            res = rumble_send_email(master, mxr->host, filename, item->sender, item->recipient);

            /* get the best result from all servers we've tried */
            delivered = (res->replyCode < delivered) ? res->replyCode : delivered;
            rumble_debug(master, "mailman", "MTA <%s> returned code %d (%s)", res->replyServer, delivered, res->replyMessage);
            sprintf(serverReply, "<%s> said: [%d] %s", res->replyServer, res->replyCode, res->replyMessage);
            rumble_flush_dictionary(res->flags);
            free(res->flags);
            free(res->replyMessage);
            free(res->replyServer);
            free(res);
            if (delivered <= 299) break;            /* yay! */
        }

        free(filename);
    }

    if (delivered >= 500) smtp_deliver_failure(master, item->sender->raw, item->recipient->raw, serverReply);
    else if (delivered >= 400) {
        // If we have tried 5 times without succeess, it's time to end this.
        if (item->loops >= maxAttempts) {
            smtp_deliver_failure(master, item->sender->raw, item->recipient->raw, serverReply);
        }
        else {
            /* temp failure, push mail back into queue (schedule next try in 6 minutes). */
            rumble_debug(master, "mailman", "MTA reported temporary error(%u), queuing mail for later (+%u secs)", delivered, retryInterval);
            sprintf(statement, "INSERT INTO queue (id,time, loops, fid, sender, recipient, flags) VALUES (NULL,strftime('%%%%s', 'now', '+%u seconds'),%%u,%%s,%%s,%%s,%%s)", retryInterval);
            if (master->_core.mail->dbType == RADB_MYSQL) {
                sprintf(statement, "INSERT INTO queue (id,time, loops, fid, sender, recipient, flags) VALUES (NULL,NOW( ) + INTERVAL %u SECOND,%%u,%%s,%%s,%%s,%%s)", retryInterval);
            }

            radb_run_inject(master->_core.mail, statement, item->loops, item->fid, item->sender->raw, item->recipient->raw, item->flags);
            rumble_debug(master, "mailman", "Mail %s queued", item->fid);
            memset(tmp, 0, 256);
        }
    } else {
        rumble_debug(master, "mailman", "Mail %s delivered.", item->fid);
    }

    /* Clean up DNS records. */
    if (mx) comm_mxFree(mx);    
    rumble_flush_dictionary(badmx);
    dvector_destroy(badmx);
    /* All done! */
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int smtp_deliver_failure(masterHandle *master, const char *sender, const char *recipient, const char *reason) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    char        *fid,
                *newfilename;
    const char  *sf;
    FILE        *fp;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    /* critical failure, giving up. */
    rumble_debug(master, "mailman", "Critical failure, letting sender know");
    fid = rumble_create_filename();
    sf = rumble_config_str(master, "storagefolder");
    newfilename = (char *) calloc(1, strlen(sf) + 26);
    sprintf(newfilename, "%s/%s", sf, fid);
    fp = fopen(newfilename, "wb");
    if (fp) {
        fprintf(fp,
                "\
To: %s\r\nFrom: Mailer Daemon <mailman@localhost>\r\n\
Subject: Delivery failed\r\n\
\r\n\
The email you sent to %s failed to be delivered.\r\n\
%s\r\n\r\n\
With regards,\r\n\
Mailer Daemon at %s on Rumble Mail Server v/%u.%u.%u\r\n",
            sender, recipient, reason, rrdict(master->_core.conf, "servername"), RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
        radb_run_inject(master->_core.mail, "INSERT INTO queue (id,loops, fid, sender, recipient, flags) VALUES (NULL,%u,%s,%s,%s,%s)", 1,
                        fid, "Mailer Daemon <mailman@localhost>", sender, "");
        fclose(fp);
    }

    free(newfilename);
    free(fid);
    return 1;
}

/*$4
 ***********************************************************************************************************************
    Private handling function for rumble_send_email()
 ***********************************************************************************************************************
 */

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
            if (!line) break;
            res->replyCode = 500;

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

    free(flag);
    if (res->replyCode == 500) ((rumbleService *) session->_svc)->traffic.rejections++;
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
    s._tflags = 0;
    s._svc = comm_serviceHandleExtern(master, "mailman");
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
    c.brecv = 0;
    c.bsent = 0;
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
            if (s._svc) ((rumbleService *) s._svc)->traffic.sent += chunk;
            else printf("..");
        }

        rcsend(&s, ".\r\n");
        get_smtp_response(&s, res);
        break;
    }

    fclose(fp);
    rcprintf(&s, "QUIT\r\n", sender);
    comm_addEntry(s._svc, c.brecv + c.bsent, 0);
    if (c.socket) disconnect(c.socket);
    return (res);
}
