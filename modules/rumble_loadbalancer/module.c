/* 
 * File:   module.c
 * Author: Humbedooh
 * 
 * A sample load balancing module.
 *
 * Created on January 3, 2011, 8:08 PM
 */

#include <string.h>
#include "../../rumble.h"
#include "../../private.h"
#include "pthread.h"

#define FOREMAN_MAX_JOBS        2 // Maximum amount of "jobs" each thread is allowed before it's destroyed
#define FOREMAN_FALLBACK        10 // Fall back to a minimum of 10 threads per service when idling
#define FOREMAN_THREAD_BUFFER   5 // Create 5 new threads whenever there's a shortage

masterHandle* m;

ssize_t smtp_hook(sessionHandle* session) {
    uint32_t workers = cvector_size(m->smtp.handles);
    uint32_t workload = (session->_tflags & 0xFFF00000) >> 20;
    if ( workload >= FOREMAN_MAX_JOBS ) {
        session->_tflags |= RUMBLE_THREAD_DIE;
        pthread_t* t = malloc(sizeof(pthread_t));
        pthread_mutex_lock(&(m->smtp.mutex));
        cvector_add(m->smtp.threads, t);
        pthread_mutex_unlock(&(m->smtp.mutex));
        pthread_create(t, NULL, rumble_smtp_init, m);
        printf("killed a thread and made a new\n");
    }
    workers = workers;
    return RUMBLE_RETURN_OKAY;
}

int rumble_module_init(void* master, rumble_module_info* modinfo) {
    modinfo->title = "Foreman module";
    modinfo->description = "Standard module for dynamically managing thread and memory pools.";
    m = master;
    // Do stuff here...
    
    // Hook the module to new SMTP connections.
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, smtp_hook);
    return EXIT_SUCCESS; // Tell rumble that the module loaded okay.
}
