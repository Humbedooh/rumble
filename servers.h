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
    
    // Connection handlers
    void rumble_clean_session(sessionHandle* session);
    
    // SMTP handlers
    ssize_t rumble_server_smtp_mail(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_rcpt(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_helo(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_ehlo(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_data(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_rset(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_vrfy(masterHandle* master, sessionHandle* session, const char* argument);
    ssize_t rumble_server_smtp_noop(masterHandle* master, sessionHandle* session, const char* argument);
    
    // Hook handlers
    ssize_t rumble_server_execute_hooks(sessionHandle* session, cvector* hooks, uint32_t flags);
    ssize_t rumble_server_schedule_hooks(masterHandle* handle, sessionHandle* session, uint32_t flags);

    void* rumble_smtp_init(void* m);
    void* rumble_pop3_init(void* m);
    void* rumble_imap_init(void* m);
    void* rumble_worker_init(void* m);

#ifdef	__cplusplus
}
#endif

#endif	/* SERVERS_H */

