/*$I0 */
<< << << < HEAD << << << < HEAD == == == =

/*$T module.c GC 1.140 02/24/11 18:54:44 */
>> >> >> > 7 c6078b307d012f3ab1c0cc605edd7fa50d50252 == == == =

/*$T module.c GC 1.140 02/24/11 18:54:44 */
>> >> >> > 43 a381c615c91573f80c48bfd2769fa03b2c5644

/*
 * File: module.c Author: Humbedooh A sample module for rumble. Created on January
 * 3, 2011, 8:08 P
 */
#include <string.h>
#include "../../rumble.h"
ssize_t sample_hook (sessionHandle * session) { }
int rumble_module_init (void *master) {

    /*
     * Do stuff here... ;
     * Hook the module to new SMTP connections
     */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, sample_hook);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}
