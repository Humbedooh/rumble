/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
#include <fcntl.h>
extern masterHandle *rumble_database_master_handle;

/*
 =======================================================================================================================
    NEW MAILMAN STUFF GOES HERE ;
    rumble_letters_open: Opens a letter as a file handle
 =======================================================================================================================
 */
FILE *rumble_letters_open(rumble_mailbox *mbox, rumble_letter *letter) {

    /*~~~~~~~~~~~~~~*/
    char    file[512];
    /*~~~~~~~~~~~~~~*/

    sprintf(file, "%s/%s.msg",
            strlen(mbox->domain->path) ? mbox->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder"),
            letter->fid);
    return (fopen(file, "rb"));
}

/*
 =======================================================================================================================
    rumble_mailman_letter_spawn: Spawns a letter from the passed DB pointer
 =======================================================================================================================
 */
rumble_letter *rumble_mailman_letter_spawn(radbResult *dbr) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter   *letter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    letter = (rumble_letter *) malloc(sizeof(rumble_letter));
    if (!letter) merror();

    /* Letter ID */
    letter->id = dbr->column[0].data.int64;

    /* Letter File ID */
    letter->fid = strclone(dbr->column[1].data.string);

    /* Letter Size */
    letter->size = dbr->column[2].data.int32;

    /* Delivery date */
    letter->delivered = dbr->column[3].data.int32;

    /* Flags */
    letter->flags = dbr->column[4].data.int32;
    letter->_flags = letter->flags;

    /* Folder */
    letter->folder = dbr->column[5].data.int64;
    return (letter);
}

/*
 =======================================================================================================================
    rumble_letters_retrieve_shared(acc): Retrieves a completed, shared instance of a mailbag for session4 use. The bag
    can be shared across multiple connections for faster processing.
 =======================================================================================================================
 */
rumble_mailman_shared_bag *rumble_letters_retrieve_shared(uint32_t uid) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                             l;
    radbObject                      *dbo;
    radbResult                      *dbr;
    rumble_mailman_shared_bag       *bag;
    rumble_letter                   *letter;
    rumble_mailman_shared_folder    *folder;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("<Mailman> Creating new bag struct for account #%u\n", uid);
    bag = (rumble_mailman_shared_bag *) malloc(sizeof(rumble_mailman_shared_bag));
    bag->folders = dvector_init();
    bag->rrw = rumble_rw_init();
    bag->uid = uid;
    bag->sessions = 1;
    rumble_rw_start_write(rumble_database_master_handle->mailboxes.rrw);
    dvector_add(rumble_database_master_handle->mailboxes.list, bag);
    rumble_rw_stop_write(rumble_database_master_handle->mailboxes.rrw);
    rumble_rw_start_write(bag->rrw);

    /* Add the default inbox */
    folder = (rumble_mailman_shared_folder *) malloc(sizeof(rumble_mailman_shared_folder));
    folder->id = 0;
    folder->letters = dvector_init();
    folder->lastMessage = 0;
    folder->updated = time(0);
    folder->subscribed = 1;
    folder->name = (char *) calloc(1, 32);
    folder->bag = bag;
    strcpy(folder->name, "INBOX");
    dvector_add(bag->folders, folder);
    dbo = radb_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", uid);
    while ((dbr = radb_step(dbo))) {
        folder = (rumble_mailman_shared_folder *) malloc(sizeof(rumble_mailman_shared_folder));
        folder->bag = bag;

        /* Folder ID */
        folder->id = dbr->column[0].data.int64;

        /* Folder name */
        folder->name = strclone(dbr->column[1].data.string);

        /* Subscribed? */
        folder->subscribed = dbr->column[2].data.int32;
        folder->letters = dvector_init();
        folder->updated = time(0);
        folder->lastMessage = 0;
        dvector_add(bag->folders, folder);
    }

    radb_cleanup(dbo);
    dbo = radb_prepare(rumble_database_master_handle->_core.mail, "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE uid = %u",
                       uid);
    while ((dbr = radb_step(dbo))) {
        letter = rumble_mailman_letter_spawn(dbr);
        letter->uid = uid;
        l = 0;
        foreach(rmsf, folder, bag->folders, iter) {
            if (folder->id == letter->folder) {
                l++;
                dvector_add(folder->letters, letter);
                folder->lastMessage = (folder->lastMessage < letter->id) ? letter->id : folder->lastMessage;
                printf("<Mailman> Set last ID in <%s> to %"PRIu64 "\n", folder->name, folder->lastMessage);
                break;
            }
        }

        if (!l) {
            free(letter->fid);
            free(letter);
        }
    }

    rumble_rw_stop_write(bag->rrw);
    radb_cleanup(dbo);
    return (bag);
}

/*
 =======================================================================================================================
    rumble_mailman_current_folder: Fetches the currently selected folder in the session.
 =======================================================================================================================
 */
rumble_mailman_shared_folder *rumble_mailman_current_folder(accountSession *sess) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_mailman_shared_folder    *folder;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!sess->bag) {
        printf("Bad call made for mailman_current_folder!\n");
        return (0);
    }

    foreach(rmsf, folder, sess->bag->folders, iter) {
        if (folder->id == sess->folder) return (folder);
    }

    return (0);
}

/*
 =======================================================================================================================
    rumble_mailman_update_folders: Updates the list of folders in the mail account
 =======================================================================================================================
 */
void rumble_mailman_update_folders(rumble_mailman_shared_bag *bag) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_mailman_shared_folder    *folder;
    int                             found;
    uint64_t                        folder_id;
    radbObject                      *dbo;
    radbResult                      *dbr;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_rw_start_write(bag->rrw);    /* Lock bag for writing */
    dbo = radb_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", bag->uid);
    while ((dbr = radb_step(dbo))) {

        /* Get the folder ID */
        folder_id = dbr->column[0].data.int64;

        /* Match against our existing folders and add if not there. */
        found = 0;
        foreach(rmsf, folder, bag->folders, iter) {
            if (folder->id == folder_id) {
                found++;
                break;
            }
        }

        if (!found) {
            folder = (rumble_mailman_shared_folder *) malloc(sizeof(rumble_mailman_shared_folder));
            if (!folder) merror();
            folder->id = folder_id;
            folder->bag = bag;

            /* Folder name */
            folder->name = strclone(dbr->column[1].data.string);

            /* Subscribed? */
            folder->subscribed = dbr->column[2].data.int32;
            dvector_add(bag->folders, folder);
        }
    }

    radb_cleanup(dbo);
    rumble_rw_stop_write(bag->rrw);     /* Unlock bag again. */
}

/*
 =======================================================================================================================
    rumble_mailman_scan_incoming: Scans for changes in the selected folder and updates it accordingly.
 =======================================================================================================================
 */
uint32_t rumble_mailman_scan_incoming(rumble_mailman_shared_folder *folder) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    int             r,
                    exists;
    radbObject      *dbo;
    radbResult      *dbr;
    rumble_letter   *letter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    /*
     * rumble_letter* letter;
     */
    if (!folder) return (0);
    r = 0;
    printf("<Mailman> Updating <%s> from ID > %"PRIu64 "\n", folder->name, folder->lastMessage);
    dbo = radb_prepare(rumble_database_master_handle->_core.mail,
                       "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE folder = %l AND id > %l", folder->id,
                       folder->lastMessage);
    rumble_rw_start_write(folder->bag->rrw);    /* Lock the bag for writing */
    while ((dbr = radb_step(dbo))) {
        r++;
        exists = 1;
        letter = rumble_mailman_letter_spawn(dbr);
        letter->uid = folder->bag->uid;
        dvector_add(folder->letters, letter);
        folder->lastMessage = (folder->lastMessage < letter->id) ? letter->id : folder->lastMessage;
        printf("Adding letter %"PRIu64 " to <%s>\n", letter->id, folder->name);
    }

    printf("<Mailman> Set last ID in <%s> to %"PRIu64 "\n", folder->name, folder->lastMessage);
    rumble_rw_stop_write(folder->bag->rrw);     /* Unlock the bag */

    /* Clean up DB */
    radb_cleanup(dbo);
    return (r);
}

/*
 =======================================================================================================================
    rumble_mailman_commit: Commits any changes done to the folder, deleting deleted letters and updating any flags set
 =======================================================================================================================
 */
uint32_t rumble_mailman_commit(accountSession *session, rumble_mailman_shared_folder *folder, int expungedOnly) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    int             r;
    radbObject      *dbo;
    const char      *path;
    rumble_letter   *letter;
    char            tmp[256];
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    if (!folder) return (0);
    path = strlen(session->account->domain->path) ? session->account->domain->path : rrdict(rumble_database_master_handle->_core.conf,
                                                                                            "storagefolder");
    r = 0;
    rumble_rw_start_write(session->bag->rrw);   /* Lock the bag */
    rumble_debug("mailman", "Cleaning up %s@%s's folder <%s>\n", session->account->user, session->account->domain->name, folder->name);
    dforeach((rumble_letter *), letter, folder->letters, iter) {

        /*
         * printf("parsing letter no. %"PRIu64 " with flags %08x\n", letter->id, letter->flags);
         */
        if ((!expungedOnly && (letter->flags & RUMBLE_LETTER_DELETED)) || (expungedOnly && (letter->flags & RUMBLE_LETTER_EXPUNGE)))
        {

            /* Delete it? */
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_STORAGE)
            printf("Deleting letter no. %"PRIu64 " (%08x -> %08x)\r\n", letter->id, letter->_flags, letter->flags);
#endif
            sprintf(tmp, "%s/%s.msg", path, letter->fid);
            unlink(tmp);
            dbo = radb_prepare(rumble_database_master_handle->_core.mail, "DELETE FROM mbox WHERE id = %l", letter->id);
            radb_step(dbo);
            radb_cleanup(dbo);

            /*
             * printf("DELETE FROM mbox WHERE id = %"PRIu64 "\n", letter->id);
             */
            r++;
            free(letter->fid);
            letter->fid = 0;
            free(letter);

            /*
             * printf("size of folder before deletion: %u", folder->letters->size);
             */
            dvector_delete(&iter);

            /*
             * printf("size of folder after deletion: %u", folder->letters->size);
             */
        } else if (letter->flags != letter->_flags)
        {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_STORAGE)
            printf("Updating letter no. %"PRIu64 " (%08x -> %08x)\r\n", letter->id, letter->_flags, letter->flags);
#endif
            if (letter->flags & RUMBLE_LETTER_UPDATED) letter->flags -= RUMBLE_LETTER_UPDATED;
            dbo = radb_prepare(rumble_database_master_handle->_core.mail, "UPDATE mbox SET flags = %u WHERE id = %l", letter->flags,
                               letter->id);
            radb_step(dbo);
            radb_cleanup(dbo);
            r++;
        }
    }

    rumble_rw_stop_write(session->bag->rrw);    /* Unlock the bag */
    return (r);
}

/*
 =======================================================================================================================
    rumble_mailman_close_bag: Frees up resources used by the bag
 =======================================================================================================================
 */
void rumble_mailman_close_bag(rumble_mailman_shared_bag *bag) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter                   *letter;
    rumble_mailman_shared_folder    *folder;
    d_iterator                      fiter,
                                    liter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!bag) return;
    rumble_rw_start_write(rumble_database_master_handle->mailboxes.rrw);    /* Lock the mailboxes */
    bag->sessions--;

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        If the session is no longer in use by any threads, destroy it and free up resources
     -------------------------------------------------------------------------------------------------------------------
     */

    if (bag->sessions <= 0) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        rumble_mailman_shared_bag   *tmpbag;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        foreach(rmsb, tmpbag, rumble_database_master_handle->mailboxes.list, fiter) {
            if (tmpbag->uid == bag->uid) {
                dvector_delete(&fiter);
                break;
            }
        }

        /* Traverse folders */
        foreach(rmsf, folder, bag->folders, fiter) {

            /* Traverse letters */
            foreach((rumble_letter *), letter, folder->letters, liter) {
                if (letter->fid) free(letter->fid);
                free(letter);
            }

            dvector_destroy(folder->letters);
            if (folder->name) free(folder->name);
            free(folder);
        }

        dvector_destroy(bag->folders);
        pthread_mutex_destroy(&bag->rrw->mutex);
        free(bag->rrw);
        free(bag);
        bag = 0;
    }

    rumble_rw_stop_write(rumble_database_master_handle->mailboxes.rrw); /* Unlock mailboxes */
}

/*
 =======================================================================================================================
    rumble_mailman_open_bag: Opens up an existing mail bag or creates a new instance of one
 =======================================================================================================================
 */
rumble_mailman_shared_bag *rumble_mailman_open_bag(uint32_t uid) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_mailman_shared_bag   *tmpbag,
                                *bag = 0;
    d_iterator                  iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_rw_start_read(rumble_database_master_handle->mailboxes.rrw); /* Lock mailboxes for writing */
    printf("looking for bag...\n");

    /* Check if we have a shared mailbox instance available */
    foreach(rmsb, tmpbag, rumble_database_master_handle->mailboxes.list, iter) {
        if (tmpbag->uid == uid) {
            bag = tmpbag;
            bag->sessions++;
            break;
        }
    }

    rumble_rw_stop_read(rumble_database_master_handle->mailboxes.rrw);

    /* If not, create a new shared object */
    if (!bag) bag = rumble_letters_retrieve_shared(uid);
    else printf("Found an old bag\n");
    return (bag);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
size_t rumble_mailman_copy_letter(rumble_mailbox *account, rumble_letter *letter, rumble_mailman_shared_folder *folder) {

    /*~~~~~~~~~~~~~~~~~~*/
    char    *path,
            *filename,
            fullname[512];
    size_t  len;
    FILE    *in,
            *out;
    char    buffer[4096];
    /*~~~~~~~~~~~~~~~~~~*/

    if (folder) {
        path = (char *) (strlen(account->domain->path) ? account->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder"));
        filename = rumble_create_filename();
        sprintf(fullname, "%s/%s.msg", path, filename);
        in = rumble_letters_open(account, letter);
        if (!in) {
            printf("couldn't open in-file <%s>\r\n", letter->fid);
            free(filename);
            return (0);
        }

        out = fopen(fullname, "wb");
        if (!out) {
            fclose(in);
            printf("couldn't open out-file <%s>\r\n", fullname);
            fclose(in);
            free(filename);
            return (0);
        }

        while (!feof(in)) {
            len = fread(buffer, 1, 4096, in);
            fwrite(buffer, 1, len, out);
        }

        fclose(in);
        fclose(out);
        radb_run_inject(rumble_database_master_handle->_core.mail,
                        "INSERT INTO mbox (id,uid, fid, folder, size, flags) VALUES (NULL,%u, %s, %l, %u, %u)", account->uid, filename,
                        folder->id, letter->size, letter->flags | RUMBLE_LETTER_RECENT);
        free(filename);
    }

    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_mailman_free_parsed_letter(rumble_parsed_letter *letter) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    c_iterator              iter;
    rumbleKeyValuePair      *pair;
    rumble_parsed_letter    *chunk;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (letter->body) free(letter->body);
    if (letter->headers) {
        cforeach((rumbleKeyValuePair *), pair, letter->headers, iter) {
            free((char *) pair->key);
            free((char *) pair->value);
        }

        cvector_destroy(letter->headers);
    }

    if (letter->multipart_chunks) {
        cforeach((rumble_parsed_letter *), chunk, letter->multipart_chunks, iter) {
            rumble_mailman_free_parsed_letter(chunk);
        }

        cvector_destroy(letter->multipart_chunks);
    }

    free(letter);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_parsed_letter *rumble_mailman_readmail(const char *filename) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_parsed_letter    *letter = 0;
    FILE                    *fp;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /*
     * printf("Mailman.readMail: Opening <%s>\n", filename);
     */
    fp = fopen(filename, "rb");
    if (fp) {
        letter = rumble_mailman_readmail_private(fp, 0, 0);
        fclose(fp);
    }

    /*
     * printf("Closing <%s>\n", filename);
     */
    return (letter);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void pdepth(int depth, const char *msg) {

    /*~~*/
    int n;
    /*~~*/

    for (n = 0; n < depth; n++) printf("    ");
    printf("%s\n", msg);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_parsed_letter *rumble_mailman_readmail_private(FILE *fp, const char *boundary, int depth) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_parsed_letter    *letter;
    rumbleKeyValuePair      *header = 0;
    int                     headers = 1;
    size_t                  blen = 0,
                            pos = 0,
                            llen = 0;
    unsigned long           previous = 0;
    char                    line[1024],
                            key[256],
                            value[1024],
                            starting_boundary[128],
                            finishing_boundary[128],
                            child_boundary[128];
    c_iterator              iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!fp) return (0);

    /*
     * printf("Parsing file pointer with boundary set to <%s>\n", boundary ? boundary : "(null)");
     * pdepth(depth, "<letter>");
     */
    letter = (rumble_parsed_letter *) malloc(sizeof(rumble_parsed_letter));
    letter->body = 0;
    letter->is_multipart = 0;
    letter->is_last_part = 0;
    letter->headers = cvector_init();
    letter->multipart_chunks = cvector_init();
    if (boundary) {
        sprintf(starting_boundary, "--%s", boundary);
        sprintf(finishing_boundary, "--%s--", boundary);
        blen = strlen(starting_boundary);
    }

    memset(child_boundary, 0, 128);
    depth++;

    /*
     * pdepth(depth, "<headers>");
     */
    while (!feof(fp)) {
        if (fgets(line, 1024, fp)) {
            llen = strlen(line);

            /* Check if we hit a boundary line. */
            if (boundary && !strncmp(line, starting_boundary, blen)) {
                if (!strncmp(line, finishing_boundary, blen + 2)) letter->is_last_part = 1; /* Did we hit the last boundary? */
                break;  /* Finish up and return. */
            }

            if (headers == 1) {
                if (!llen || line[0] == '\r' || line[0] == '\n') {
                    headers = 0;

                    /*
                     * pdepth(depth, "</headers>");
                     */
                    continue;
                }       /* End of headers */

                memset(key, 0, 256);
                memset(value, 0, 1024);

                /* Are we continuing the previous header line? */
                if (header && (line[0] == ' ' || line[0] == '\t')) {
                    if (sscanf(line, "%1024[^\r\n]", value) == 1) {

                        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                        size_t  old_len = strlen(header->value),
                                new_len = strlen(value);
                        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                        value[new_len] = 0;

                        /*
                         * printf("Reallocating value to hold %lu bytes\n", old_len+new_len+1);
                         */
                        header->value = realloc((char *) header->value, old_len + new_len + 1);
                        strncpy((char *) header->value + old_len, line, new_len + 1);

                        /*
                         * printf("+%s: %s\n", header->key, line);
                         */
                    }
                }

                /* Or did we start on a new header pair? */
                if (sscanf(line, "%256[^:]: %1024[^\r\n]", key, value) == 2) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    size_t  klen = strlen(key),
                            vlen = strlen(value);
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    header = (rumbleKeyValuePair *) malloc(sizeof(rumbleKeyValuePair));
                    header->key = calloc(1, klen + 1);
                    rumble_string_lower(key);
                    strncpy((char *) header->key, key, klen);
                    header->value = calloc(1, vlen + 1);
                    strncpy((char *) header->value, value, vlen);
                    cvector_add(letter->headers, header);

                    /*
                     * printf("%s: %s\n", key, value);
                     * pdepth(depth+1, "<header>");
                     * pdepth(depth+1, line);
                     */
                }
            } /* Done with headers, browse through them and look for a boundary if any */ else if (headers == 0) {

                /*
                 * pdepth(depth, "<header check>");
                 */
                cforeach((rumbleKeyValuePair *), header, letter->headers, iter) {
                    if (!strcmp(header->key, "content-type")) {

                        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                        /*
                         * printf("Found a content-type: %s\n", header->value);
                         */
                        const char  *at = strstr(header->value, "boundary=");
                        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                        if (at && sscanf(at, "boundary=\"%255[^\"]", child_boundary)) {

                            /*
                             * printf("This message has boundary issues! ;
                             * D (%s)\n",child_boundary);
                             */
                            letter->is_multipart = 1;
                        }
                    }
                }

                headers = -1;

                /*
                 * if (!letter->is_multipart) printf("Mail is single-part\n");
                 * ;
                 * pdepth(depth, "</header check>");
                 * pdepth(depth, "<body>");
                 */
            }

            /* Read body of message */
            if (headers == -1) {

                /* No multipart, just add the body. */
                if (!letter->is_multipart) {

                    /*
                     * printf(".");
                     * New body, malloc it
                     */
                    if (!letter->body) {
                        letter->body = (char *) calloc(1, llen + 1);
                        strncpy(letter->body, line, llen);
                        pos = llen;
                    } /* Old body, append to it. */ else {
                        letter->body = (char *) realloc((char *) letter->body, pos + llen + 1);
                        strncpy((char *) letter->body + pos, line, llen);
                        pos += llen;
                        letter->body[pos] = 0;
                    }
                } else {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                    rumble_parsed_letter    *child = 0;
                    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                    fseek(fp, previous, SEEK_SET);

                    /*
                     * printf("This line was: %s\n", line);
                     */
                    while (1) {
                        child = rumble_mailman_readmail_private(fp, child_boundary, depth);
                        if (!child) break;
                        cvector_add(letter->multipart_chunks, child);
                        if (child->is_last_part) break;
                    }

                    if (child and child->is_last_part) break;
                }
            }

            previous = ftell(fp);
        }
    }

    /*
     * printf("\n");
     * pdepth(depth, "</body>");
     * pdepth(--depth, "</letter>");
     * if (letter->is_last_part) pdepth(--depth, "<last chunk>");
     */
    if (letter->body || letter->is_multipart) {
        return (letter);
    } else {
        rumble_mailman_free_parsed_letter(letter);
        return (0);
    }
}
