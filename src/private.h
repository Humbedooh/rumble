/* 
 * File:   private.h
 * Author: Administrator
 *
 * Private functions for rumble
 * Created on January 9, 2011, 6:53 PM
 */

#ifndef PRIVATE_H
#define	PRIVATE_H
#include "rumble.h"
#include "servers.h"

#ifdef	__cplusplus
extern "C" {
#endif

void rumble_config_load(masterHandle* master);
void rumble_master_init(masterHandle* master);
void rumble_modules_load(masterHandle* master);
void rumble_database_load(masterHandle* master);
masterHandle* rumble_get_master();

#ifdef	__cplusplus
}
#endif

#endif	/* PRIVATE_H */

