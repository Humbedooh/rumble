/* File: mailman.h Author: vaps Created on 20. december 2011, 13:46 */
#ifndef MAILMAN_H
#   define MAILMAN_H
#   ifdef __cplusplus
extern "C"
{
#   endif
#   include "rumble.h"
#   include "private.h"
extern masterHandle *rumble_database_master_handle;
mailman_folder      *mailman_new_folder(mailman_bag *bag);
mailman_bag         *mailman_new_bag(uint32_t uid, const char *path);
mailman_letter      *mailman_new_letter(mailman_folder *folder);
void                mailman_free_folder(mailman_folder *folder);
void                mailman_free_bag(mailman_bag *bag);
void                mailman_close_bag(mailman_bag *bag);
void                mailman_add_flags(mailman_folder *folder, uint32_t flags, uint32_t UID, uint64_t start, uint64_t stop);
void                mailman_remove_flags(mailman_folder *folder, uint32_t flags, uint32_t UID, uint64_t start, uint64_t stop);
void                mailman_set_flags(mailman_folder *folder, uint32_t flags, uint32_t UID, uint64_t start, uint64_t stop);
void                mailman_update_folders(mailman_bag *bag);
void                mailman_update_folder(mailman_folder *folder, uint32_t uid, uint64_t lastID);
mailman_folder      *mailman_get_folder(mailman_bag *bag, const char *name);
void                mailman_rename_folder(mailman_folder *folder, const char *name);
void                mailman_delete_folder(mailman_bag *bag, mailman_folder *folder);
void                mailman_commit(mailman_bag *bag, mailman_folder *folder, char expungeOnly);
FILE                *mailman_open_letter(mailman_bag *bag, mailman_folder *folder, uint64_t id);
void                mailman_copy_letter
                    (
                        mailman_bag     *bag,
                        mailman_folder  *sourceFolder,
                        mailman_folder  *destFolder,
                        uint64_t        start,
                        uint64_t        stop,
                        uint32_t        UID
                    );
mailman_bag         *mailman_get_bag(uint32_t uid, const char *path);
int                 smtp_deliver_feed(rumble_mailbox *user, mqueue *item, masterHandle *master);
int                 smtp_deliver_alias(rumble_mailbox *user, mqueue *item, masterHandle *master);
int                 smtp_deliver_mbox(rumble_mailbox *user, mqueue *item, masterHandle *master);
int                 smtp_deliver_foreign(mqueue *item, masterHandle *master, const char *host);
int                 smtp_deliver_failure(masterHandle *master, const char *sender, const char *recipient, const char *reason);
#   ifdef __cplusplus
}
#   endif
#endif /* MAILMAN_H */
