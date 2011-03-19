/*
 * File: private.h Author: Administrator Private functions for rumble Created on
 * January 9, 2011, 6:53 PM
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
void                rumble_modules_load(masterHandle *master, FILE *runlog);
void                rumble_database_load(masterHandle *master, FILE *runlog);
masterHandle        *rumble_get_master(void);
void                rumble_tag_file(FILE *fp, const char *host, const char *fid, const char *usr, const char *dmn);
size_t              rumble_copy_mail(masterHandle *master, const char *fid, const char *usr, const char *dmn, char **pfid);
rumble_readerwriter *rumble_rw_init(void);
void                rumble_rw_start_read(rumble_readerwriter *rrw);
void                rumble_rw_stop_read(rumble_readerwriter *rrw);
void                rumble_rw_start_write(rumble_readerwriter *rrw);
void                rumble_rw_stop_write(rumble_readerwriter *rrw);
#   ifdef __cplusplus
}
#   endif
#endif /* PRIVATE_H */
