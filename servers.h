/* 
 * File:   servers.h
 * Author: Administrator
 *
 * Created on January 6, 2011, 3:39 PM
 */

#ifndef SERVERS_H
#define	SERVERS_H

#include "rumble.h"

#ifdef	__cplusplus
extern "C" {
#endif
    
ssize_t rumble_server_execute_hooks(sessionHandle* session, cvector* hooks, unsigned short flags);
ssize_t rumble_server_schedule_hooks(masterHandle* handle, sessionHandle* session, unsigned short flags);

void* rumble_smtp_init(void* m);
void* rumble_pop3_init(void* m);
void* rumble_imap_init(void* m);
void* rumble_worker_init(void* m);

#ifdef	__cplusplus
}
#endif

#endif	/* SERVERS_H */

