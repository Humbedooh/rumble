/*
 * File: private.h Author: Administrator Private functions for rumble Created on January 9, 2011, 6:53
 * P
 */
#ifndef PRIVATE_H
#   define PRIVATE_H
#   include "rumble.h"
#   include "servers.h"
#   ifdef __cplusplus
extern "C"
{
#   endif
void                rumble_config_load(masterHandle *master, dvector *args);
void                rumble_master_init(masterHandle *master);
void                rumble_modules_load(masterHandle *master);
void                rumble_database_load(masterHandle *master, FILE *runlog);
masterHandle        *rumble_get_master(void);
void                rumble_tag_file(FILE *fp, const char *host, const char *fid, const char *usr, const char *dmn);
size_t              rumble_copy_mail(masterHandle *master, const char *fid, const char *usr, const char *dmn, char **pfid);
size_t              rumble_mail_from_file(masterHandle *master, const char *oldfile, char **newfile);
rumble_readerwriter *rumble_rw_init(void);
void                rumble_rw_start_read(rumble_readerwriter *rrw);
void                rumble_rw_stop_read(rumble_readerwriter *rrw);
void                rumble_rw_start_write(rumble_readerwriter *rrw);
void                rumble_rw_stop_write(rumble_readerwriter *rrw);
void                rumble_rw_destroy(rumble_readerwriter *rrw);
char                base64_encode_value(char value_in);
int                 base64_encode_block(const char *plaintext_in, size_t length_in, char *code_out, base64_encodestate *state_in);
int                 base64_encode_blockend(char *code_out, base64_encodestate *state_in);
int                 rumbleStart(void);
void attach_debug(void);
void cleanup(void);
void windows_service_start(void);
#   ifdef __cplusplus
}
#   endif
#endif /* PRIVATE_H */
