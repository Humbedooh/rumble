/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
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
rumble_letter *rumble_mailman_letter_spawn(void *state) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    rumble_letter   *letter;
    int             l;
    /*~~~~~~~~~~~~~~~~~~~~*/

    letter = (rumble_letter *) malloc(sizeof(rumble_letter));
    if (!letter) merror();

    /* Letter ID */
    letter->id = sqlite3_column_int64((sqlite3_stmt *) state, 0);

    /* Letter File ID */
    l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
    letter->fid = (char *) calloc(1, l + 1);
    memcpy((char *) letter->fid, sqlite3_column_text((sqlite3_stmt *) state, 1), l);

    /* Letter Size */
    letter->size = sqlite3_column_int((sqlite3_stmt *) state, 2);

    /* Delivery date */
    letter->delivered = sqlite3_column_int((sqlite3_stmt *) state, 3);

    /* Flags */
    letter->flags = sqlite3_column_int((sqlite3_stmt *) state, 4);
    letter->_flags = letter->flags;

    /* Folder */
    letter->folder = sqlite3_column_int64((sqlite3_stmt *) state, 5);
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
    int                             rc,
                                    l;
    void                            *state;
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
    dvector_add(rumble_database_master_handle->mailboxes.list, bag);

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
    state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", uid);
    while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {
        folder = (rumble_mailman_shared_folder *) malloc(sizeof(rumble_mailman_shared_folder));
        folder->bag = bag;
        /* Folder ID */
        folder->id = sqlite3_column_int64((sqlite3_stmt *) state, 0);

        /* Folder name */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
        folder->name = (char *) calloc(1, l + 1);
        strncpy(folder->name, (char *) sqlite3_column_text((sqlite3_stmt *) state, 1), l);

        /* Subscribed? */
        folder->subscribed = sqlite3_column_int((sqlite3_stmt *) state, 2);
        folder->letters = dvector_init();
        folder->updated = time(0);
        folder->lastMessage = 0;
        dvector_add(bag->folders, folder);
    }

    rumble_database_cleanup(state);
    state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                    "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE uid = %u", uid);
    while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {
        letter = rumble_mailman_letter_spawn(state);
        letter->uid = uid;
        l = 0;
        foreach(rmsf, folder, bag->folders, iter) {
            if (folder->id == letter->folder) {
                l++;
                dvector_add(folder->letters, letter);
                folder->lastMessage = (folder->lastMessage < letter->id) ? letter->id : folder->lastMessage;
                printf("<Mailman> Set last ID in <%s> to %llu\n", folder->name, folder->lastMessage);
                break;
            }
            
        }

        if (!l) {
            free(letter->fid);
            free(letter);
        }
    }

    rumble_database_cleanup(state);
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
    int                             rc,
                                    l,
                                    folder_id,
                                    found;
    void                            *state;
    d_iterator                      iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_rw_start_write(bag->rrw);    /* Lock bag for writing */
    state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u",
                                    bag->uid);
    while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {

        /* Get the folder ID */
        folder_id = sqlite3_column_int((sqlite3_stmt *) state, 0);

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
            l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
            folder->name = (char *) calloc(1, l + 1);
            memcpy((char *) folder->name, sqlite3_column_text((sqlite3_stmt *) state, 1), l);

            /* Subscribed? */
            folder->subscribed = sqlite3_column_int((sqlite3_stmt *) state, 2);
            
            dvector_add(bag->folders, folder);
        }
    }

    rumble_database_cleanup(state);
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
                    rc,
                    exists;
    void            *state;
    rumble_letter   *letter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    /*
     * rumble_letter* letter;
     */
    if (!folder) return (0);
    r = 0;
    printf("<Mailman> Updating <%s> from ID > %llu\n", folder->name, folder->lastMessage);
    state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                    "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE folder = %l AND id > %l", folder->id,
                                    folder->lastMessage);
    rumble_rw_start_write(folder->bag->rrw);    /* Lock the bag for writing */
    while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {
        r++;
        exists = 1;
        letter = rumble_mailman_letter_spawn(state);
        letter->uid = folder->bag->uid;
        dvector_add(folder->letters, letter);
        folder->lastMessage = (folder->lastMessage < letter->id) ? letter->id : folder->lastMessage;
        printf("Adding letter %llu to <%s>\n", letter->id, folder->name);
    }
    printf("<Mailman> Set last ID in <%s> to %llu\n", folder->name, folder->lastMessage);
    rumble_rw_stop_write(folder->bag->rrw);     /* Unlock the bag */

    /* Clean up DB */
    rumble_database_cleanup(state);
    return (r);
}

/*
 =======================================================================================================================
    rumble_mailman_commit: Commits any changes done to the folder, deleting deleted letters and updating any flags set
 =======================================================================================================================
 */
uint32_t rumble_mailman_commit(accountSession *session, rumble_mailman_shared_folder *folder) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    int             r;
    void            *state;
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
    printf("Running COMMIT on <%s>\n", folder->name);
    dforeach((rumble_letter *), letter, folder->letters, iter) {
        printf("parsing letter no. %llu with flags %08x\n", letter->id, letter->flags);
        if ((letter->flags & RUMBLE_LETTER_DELETED))
        {

            /* Delete it? */
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_STORAGE)
            printf("Deleting letter no. %llu (%08x -> %08x)\r\n", letter->id, letter->_flags, letter->flags);
#endif
            sprintf(tmp, "%s/%s.msg", path, letter->fid);
            unlink(tmp);
            state = rumble_database_prepare(rumble_database_master_handle->_core.db, "DELETE FROM mbox WHERE id = %l", letter->id);
            rumble_database_run(state);
            rumble_database_cleanup(state);
            printf("DELETE FROM mbox WHERE id = %llu\n", letter->id);
            r++;
            free(letter->fid);
            letter->fid = 0;
            free(letter);
            printf("size of folder before deletion: %u", folder->letters->size);
            dvector_delete(&iter);
            printf("size of folder after deletion: %u", folder->letters->size);
        } else if (letter->flags != letter->_flags)
        {
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_STORAGE)
            printf("Updating letter no. %llu (%08x -> %08x)\r\n", letter->id, letter->_flags, letter->flags);
#endif
            if (letter->flags & RUMBLE_LETTER_UPDATED) letter->flags -= RUMBLE_LETTER_UPDATED;
            state = rumble_database_prepare(rumble_database_master_handle->_core.db, "UPDATE mbox SET flags = %u WHERE id = %l",
                                            letter->flags, letter->id);
            rumble_database_run(state);
            rumble_database_cleanup(state);
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

        /* Traverse folders */
        foreach(rmsf, folder, bag->folders, fiter) {

            /* Traverse letters */
            foreach((rumble_letter *), letter, folder->letters, liter) {
                if (!letter) {
                    printf("Memory corruption??!\n");
                    continue;
                }

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

    rumble_rw_start_write(rumble_database_master_handle->mailboxes.rrw);    /* Lock mailboxes for writing */

    /* Check if we have a shared mailbox instance available */
    foreach(rmsb, tmpbag, rumble_database_master_handle->mailboxes.list, iter) {
        if (tmpbag->uid == uid) {
            bag = tmpbag;
            bag->sessions++;
            break;
        }
    }

    /* If not, create a new shared object */
    if (!bag) {
        bag = rumble_letters_retrieve_shared(uid);
        bag->sessions = 1;
        bag->uid = uid;
        dvector_add(rumble_database_master_handle->mailboxes.list, bag);
    }

    rumble_rw_stop_write(rumble_database_master_handle->mailboxes.rrw);     /* Unlock mailboxes */
    return (bag);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_mailman_copy_letter(rumble_mailbox *account, rumble_letter *letter, rumble_mailman_shared_folder *folder) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char            *path,
                    *filename,
                    fullname[512];
    int             len;
    FILE            *in,
                    *out;
    char            buffer[4096];
    void            *state;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
        state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                        "INSERT INTO mbox (uid, fid, folder, size, flags) VALUES (%u, %s, %l, %u, %u)", account->uid,
                                        filename, folder->id, letter->size, 0);
        rumble_database_run(state);
        rumble_database_cleanup(state);
        free(filename);
    }

    return (1);
}
