/* 
 * File:   servers.h
 * Author: Administrator
 *
 * Created on January 6, 2011, 3:39 PM
 */

#ifndef SERVERS_H
#define	SERVERS_H

#ifdef	__cplusplus
extern "C" {
#endif

void* rumble_smtp_init(void* m);
void* rumble_pop3_init(void* m);
void* rumble_imap_init(void* m);
void* rumble_worker_init(void* m);

#ifdef	__cplusplus
}
#endif

#endif	/* SERVERS_H */

