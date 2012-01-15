/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
#include <fcntl.h>
#include "mailman.h"
extern masterHandle *rumble_database_master_handle;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
mailman_folder *mailman_new_folder(mailman_bag *bag) {

    /*~~~~~~~~~~~~~~*/
    int         f,
                x = 0;
    uint32_t    i;
    /*~~~~~~~~~~~~~~*/

    if (!bag) return (0);
    f = 0;

    /* Look for an empty slot to add the folder */
    for (i = bag->firstFree; i < bag->size; i++) {
        if (bag->folders[i].inuse == 0) {
            f = i;
            x++;
            break;
        }
    }

    if (!x) {
        bag->folders = (mailman_folder *) realloc((void *) bag->folders, (bag->size + 8) * sizeof(mailman_folder));
        if (!bag->folders) printf("WTF?!\n");
        for (i = bag->size - 1; i < (bag->size + 8); i++) {
            bag->folders[i].inuse = 0;
        }

        f = bag->size;
        bag->size += 8;
    }

    bag->firstFree = f + 1;
    bag->folders[f].letters = (mailman_letter *) calloc(257, sizeof(mailman_letter));
    bag->folders[f].firstFree = 0;
    bag->folders[f].size = 256;
    bag->folders[f].lock = bag->lock;
    bag->folders[f].subscribed = 0;
    bag->folders[f].inuse = 1;
    return (&bag->folders[f]);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
mailman_bag *mailman_new_bag(uint32_t uid, const char *path) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    mailman_folder  *inbox;
    mailman_bag     *bag = (mailman_bag *) malloc(sizeof(mailman_bag));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    bag->uid = uid;
    bag->lock = rumble_rw_init();
    bag->sessions = 1;
    bag->closed = 0;
    bag->size = 32;
    bag->firstFree = 0;
    bag->folders = (mailman_folder *) calloc(33, sizeof(mailman_folder));
    inbox = mailman_new_folder(bag);
    inbox->fid = 0;
    strcpy(inbox->name, "INBOX");
    memset(bag->path, 0, 256);
    strncpy(bag->path, path, 256);
    mailman_update_folders(bag);
    return (bag);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
mailman_letter *mailman_new_letter(mailman_folder *folder) {

    /*~~~~~~~~~~~~~~*/
    int         f,
                x = 0;
    uint32_t    i;
    /*~~~~~~~~~~~~~~*/

    if (!folder) return (0);

    /* Look for an empty spot first */
    for (i = folder->firstFree; i < folder->size; i++) {
        if (folder->letters[i].inuse == 0) {
            f = i;
            x++;
            folder->letters[i].inuse = 1;
            break;
        }
    }

    if (!x) {
        folder->letters = (mailman_letter *) realloc(folder->letters, (sizeof(mailman_letter)) * (folder->size + 33));
        for (i = folder->size - 1; i < folder->size + 32; i++) {
            folder->letters[i].inuse = 0;
        }

        f = folder->size;
        folder->firstFree = f + 1;
        folder->size += 32;
    }

    return (&folder->letters[f]);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_free_folder(mailman_folder *folder) {
    if (!folder || !folder->inuse) return;
    if (!folder) return;
    if (folder->letters) free(folder->letters);
    folder->size = 0;
    folder->letters = 0;
    folder->inuse = 0;
    folder->fid = 0;
    folder->firstFree = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_free_bag(mailman_bag *bag) {

    /*~~~~~~~~~~~~~~~~~~*/
    uint32_t    i;
    mailman_bag *rbag = 0;
    c_iterator  iter;
    /*~~~~~~~~~~~~~~~~~~*/

    if (!bag) return;
    for (i = 0; i < bag->size; i++) {
        mailman_free_folder(&bag->folders[i]);
    }

    free(bag->folders);
    rumble_rw_destroy(bag->lock);
    cforeach((mailman_bag *), rbag, rumble_database_master_handle->mailboxes.bags, iter) {
        if (rbag == bag) {
            cvector_delete(&iter);
            break;
        }
    }

    free(bag);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_close_bag(mailman_bag *bag) {
    if (!bag) return;
    rumble_rw_start_write(bag->lock);
    bag->sessions--;
    if (bag->sessions == 0) bag->closed = 1;
    rumble_rw_stop_write(bag->lock);
    if (bag->sessions == 0) mailman_free_bag(bag);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_add_flags(mailman_folder *folder, uint32_t flags, uint32_t UID, uint64_t start, uint64_t stop) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        i;
    mailman_letter  *letter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (!folder) return;
    if (stop == 0) stop = start;
    rumble_rw_start_write(folder->lock);
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (letter->inuse) {
            if (UID) {
                if (letter->id >= start && letter->id <= stop) {
                    letter->flags |= flags;
                    letter->updated = 1;
                    printf("Updated flags for msg %lu: %08X\n", letter->id, letter->flags);
                }
            } else {
                if (i + 1 >= start && i < stop) {
                    letter->flags |= flags;
                    letter->updated = 1;
                }
            }
        }
    }

    rumble_rw_stop_write(folder->lock);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_remove_flags(mailman_folder *folder, uint32_t flags, uint32_t UID, uint64_t start, uint64_t stop) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        i;
    mailman_letter  *letter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (!folder) return;
    if (stop == 0) stop = start;
    rumble_rw_start_write(folder->lock);
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (letter->inuse) {
            if (UID) {
                if (letter->id >= start && letter->id <= stop) {
                    letter->flags -= (letter->flags & flags);
                    letter->updated = 1;
                    printf("Set flags for %lu to %08X\n", letter->id, letter->flags);
                }
            } else {
                if (i + 1 >= start && i < stop) {
                    letter->flags -= (letter->flags & flags);
                    letter->updated = 1;
                }
            }
        }
    }

    rumble_rw_stop_write(folder->lock);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_set_flags(mailman_folder *folder, uint32_t flags, uint32_t UID, uint64_t start, uint64_t stop) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        i;
    mailman_letter  *letter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (!folder) return;
    if (stop == 0) stop = start;
    rumble_rw_start_write(folder->lock);
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (letter->inuse) {
            if (UID) {
                if (letter->id >= start && letter->id <= stop) {
                    letter->flags = flags;
                    letter->updated = 1;
                    printf("Set flags for %lu to %08X\n", letter->id, letter->flags);
                }
            } else {
                if (i + 1 >= start && i < stop) {
                    letter->flags = flags;
                    letter->updated = 1;
                }
            }
        }
    }

    rumble_rw_stop_write(folder->lock);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_update_folders(mailman_bag *bag) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    radbObject      *dbo;
    radbResult      *dbr;
    mailman_folder  *folder;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~*/

    dbo = radb_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", bag->uid);
    while ((dbr = radb_step(dbo))) {
        folder = 0;
        for (i = 0; i < bag->size; i++) {
            if (bag->folders[i].inuse && bag->folders[i].fid == dbr->column[0].data.uint64) {
                folder = &bag->folders[i];
                break;
            }
        }

        if (!folder) {
            folder = mailman_new_folder(bag);
            folder->fid = dbr->column[0].data.uint64;
            folder->subscribed = dbr->column[2].data.int32;
            memset(folder->name, 0, 64);
            strncpy(folder->name, dbr->column[1].data.string, 64);
        }
    }

    radb_cleanup(dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_update_folder(mailman_folder *folder, uint32_t uid, uint64_t lastID) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    radbObject      *dbo;
    radbResult      *dbr;
    mailman_letter  *letter;
    int             f,
                    lid;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~*/

    dbo = radb_prepare(rumble_database_master_handle->_core.mail,
                       "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE uid = %u AND folder = %l", uid, folder->fid);
    while ((dbr = radb_step(dbo))) {
        f = 0;
        lid = dbr->column[0].data.int64;
        for (i = 0; i < folder->size; i++) {
            if (folder->letters[i].inuse) {
                if (lid == folder->letters[i].id) {
                    f++;
                    break;
                }
            }
        }

        if (!f) {
            letter = mailman_new_letter(folder);
            letter->flags = dbr->column[4].data.uint32;
            letter->id = dbr->column[0].data.uint64;
            letter->size = dbr->column[2].data.int32;
            letter->delivered = dbr->column[3].data.int32;
            letter->updated = 0;
            letter->inuse = 1;
            memset(letter->filename, 0, 32);
            strcpy(letter->filename, dbr->column[1].data.string);
            f = 0;
        }
    }

    radb_cleanup(dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
mailman_folder *mailman_get_folder(mailman_bag *bag, const char *name) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~*/
    mailman_folder  *folder = 0;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!bag || !name) return (0);
    rumble_rw_start_read(bag->lock);
    for (i = 0; i < bag->size; i++) {
        if (bag->folders[i].inuse && !strcmp(bag->folders[i].name, name)) {
            folder = &bag->folders[i];
            break;
        }
    }

    rumble_rw_stop_read(bag->lock);
    return (folder);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_rename_folder(mailman_folder *folder, const char *name) {
    if (!folder or!name) return;
    memset(folder->name, 0, 64);
    strncpy(folder->name, name, 64);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_delete_folder(mailman_bag *bag, mailman_folder *folder) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        i,
                    f;
    char            filename[512];
    mailman_letter  *letter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!bag or!folder) return;
    rumble_debug(NULL, "mailman", "Deleting account #%u's folder <%s>", bag->uid, folder->name);
    rumble_rw_start_write(folder->lock);
    radb_run_inject(rumble_database_master_handle->_core.db, "DELETE FROM folders WHERE id = %l", folder->fid);
    radb_run_inject(rumble_database_master_handle->_core.mail, "DELETE FROM mbox WHERE uid = %u AND folder = %l", bag->uid, folder->fid);
    f = 0;
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (letter->inuse) {
            f++;
            memset(filename, 0, 512);
            sprintf(filename, "%s/%s.msg", bag->path, letter->filename);
            unlink(filename);
            letter->inuse = 0;
        }
    }

    mailman_free_folder(folder);
    rumble_rw_stop_write(bag->lock);
    rumble_debug(NULL, "mailman", "Deleted %u letters.", f);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_commit(mailman_bag *bag, mailman_folder *folder, char expungeOnly) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        i;
    int             f;
    char            filename[512];
    mailman_letter  *letter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("mailman_commit()\n");
    if (!bag or!folder) return;
    rumble_debug(NULL, "mailman", "Updating #%u's folder <%s>", bag->uid, folder->name);
    rumble_rw_start_write(folder->lock);
    f = 0;
    for (i = 0; i < folder->size; i++) {
        letter = &folder->letters[i];
        if (letter->inuse) {

            
            printf("Letter %lu has flags <%08X>, looking for <%08X>\n", letter->id, letter->flags, RUMBLE_LETTER_DELETED);
            
            if ((expungeOnly && (letter->flags & RUMBLE_LETTER_EXPUNGE)) || (!expungeOnly && (letter->flags & RUMBLE_LETTER_DELETED))) {
                printf("Deleting letter no. %lu\n", letter->id);
                radb_run_inject(rumble_database_master_handle->_core.mail, "DELETE FROM mbox WHERE id = %l", letter->id);
                memset(filename, 0, 512);
                sprintf(filename, "%s/%s.msg", bag->path, letter->filename);
                unlink(filename);
                folder->letters[i].inuse = 0;
                if (i < folder->firstFree) folder->firstFree = i;
                f++;
            } else if (letter->updated) {
                radb_run_inject(rumble_database_master_handle->_core.mail, "UPDATE mbox SET flags = %u WHERE uid = %u AND id = %l",
                                letter->flags, bag->uid, letter->id);
                letter->updated = 0;
            }
        }
    }

    rumble_rw_stop_write(folder->lock);
    rumble_debug(NULL, "mailman", "Deleted %u letters.", f);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
FILE *mailman_open_letter(mailman_bag *bag, mailman_folder *folder, uint64_t id) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            filename[256];
    mailman_letter  *letter = 0;
    uint32_t        i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    for (i = 0; i < folder->size; i++) {
        if (folder->letters[i].inuse && folder->letters[i].id == id) {
            letter = &folder->letters[i];
            break;
        }
    }

    if (!letter) return (0);
    memset(filename, 0, 256);
    sprintf(filename, "%s/%s.msg", bag->path, letter->filename);
    printf("Opening %s\n", filename);
    return (fopen(filename, "r"));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void mailman_copy_letter(
    mailman_bag     *bag,
    mailman_folder  *sourceFolder,
    mailman_folder  *destFolder,
    uint64_t        start,
    uint64_t        stop,
    uint32_t        UID) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    uint32_t        i;
    mailman_letter  *letter;
    char            *fid,
                    filename[256];
    FILE            *in,
                    *out;
    char            buffer[4096];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("mailman_copy_letter()\n");
    if (!sourceFolder || !destFolder || !bag) return;
    if (stop == 0) stop = start;
    rumble_rw_start_write(bag->lock);
    for (i = 0; i < sourceFolder->size; i++) {
        letter = &sourceFolder->letters[i];
        if (letter->inuse) {
            if ((UID && (letter->id >= start && letter->id <= stop)) || (!UID && (i + 1 >= start && i < i))) {
                rumble_debug(NULL, "mailman", "Copying letter %lu to folder %lu", letter->id, destFolder->fid);
                fid = rumble_create_filename();
                sprintf(filename, "%s/%s.msg", bag->path, fid);
                in = mailman_open_letter(bag, sourceFolder, letter->id);
                if (in) {
                    out = fopen(filename, "wb");
                    if (!out) fclose(in);
                    else {

                        /*~~~~~~~~*/
                        size_t  len;
                        /*~~~~~~~~*/

                        while (!feof(in)) {
                            len = fread(buffer, 1, 4096, in);
                            fwrite(buffer, 1, len, out);
                        }

                        fclose(out);
                        radb_run_inject(rumble_database_master_handle->_core.mail,
                                        "INSERT INTO mbox (id,uid, fid, folder, size, flags) VALUES (NULL,%u, %s, %l, %u, %u)", bag->uid,
                                        fid, destFolder->fid, letter->size, letter->flags | RUMBLE_LETTER_RECENT);
                    }
                } else {
                    rumble_debug(NULL, "mailman", "Couldn't create copy of letter at %s, aborting.", filename);
                }

                free(fid);
            }
        }
    }

    rumble_rw_stop_write(bag->lock);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
mailman_bag *mailman_get_bag(uint32_t uid, const char *path) {

    /*~~~~~~~~~~~~~~~~~~*/
    mailman_bag *bag = 0;
    mailman_bag *rbag = 0;
    c_iterator  iter;
    /*~~~~~~~~~~~~~~~~~~*/

    cforeach((mailman_bag *), bag, rumble_database_master_handle->mailboxes.bags, iter) {
        if (bag->uid == uid) {
            rumble_rw_start_write(bag->lock);
            if (bag->closed == 0) rbag = bag;
            bag->sessions++;
            rumble_rw_stop_write(bag->lock);
            break;
        }
    }

    if (rbag) rumble_debug(NULL, "mailman", "Using already opened bag");
    if (!rbag) {
        rumble_debug(NULL, "mailman", "Making new bag struct with %s as path", path);
        rbag = mailman_new_bag(uid, path);
        cvector_add(rumble_database_master_handle->mailboxes.bags, rbag);
    }

    return (rbag);
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
                     * New body, malloc
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
