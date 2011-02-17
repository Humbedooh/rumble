<<<<<<< HEAD
=======
/*$T module.c GC 1.140 02/16/11 21:04:57 */

>>>>>>> 7c6078b307d012f3ab1c0cc605edd7fa50d50252
/*
 * File: module.c Author: Humbedooh A sample module for rumble. Created on January
 * 3, 2011, 8:08 P
 */
#include <string.h>
#include "../../rumble.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t sample_hook(sessionHandle *session)
{ }

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int rumble_module_init(void *master) {

    /*
     * Do stuff here... ;
     * Hook the module to new SMTP connections.
     */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, sample_hook);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
