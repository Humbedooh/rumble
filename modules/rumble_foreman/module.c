/* 
 * File:   module.c
 * Author: Humbedooh
 * 
 * A sample load balancing module.
 *
 * Created on January 3, 2011, 8:08 PM
 */

#include "../../rumble.h"
#include "../../private.h"
#include "pthread.h"

#define FOREMAN_MAX_JOBS        250 // Maximum amount of "jobs" each worker is allowed before it's destroyed
#define FOREMAN_FALLBACK        10 // Fall back to a minimum of 10 workers per service when idling
#define FOREMAN_THREAD_BUFFER   5 // Create 5 new workers whenever there is a shortage

masterHandle* m;

ssize_t accept_hook(sessionHandle* session) {
    uint32_t workload = (session->_tflags & 0xFFF00000) >> 20;
    // If this thread is getting old, kill it.
    if ( workload > FOREMAN_MAX_JOBS ) session->_tflags |= RUMBLE_THREAD_DIE;
    
    // Find out what service we're dealing with here.
    rumbleService* svc = NULL;
    switch((session->_tflags & RUMBLE_THREAD_SVCMASK)) {
        case RUMBLE_THREAD_SMTP: svc = &m->smtp; break;
        case RUMBLE_THREAD_POP3: svc = &m->pop3; break;
        case RUMBLE_THREAD_IMAP: svc = &m->imap; break;
        default: break;
    }
    if ( svc ) {
        /* Check if there's a shortage of workers.
         * If there is, make some more, if not, just return.
         */ 
        pthread_mutex_lock(&(svc->mutex));
        uint32_t workers = cvector_size(svc->threads);
        uint32_t busy = cvector_size(svc->handles);
        uint32_t idle = workers - busy;
        if ( idle <= 1 || workers < FOREMAN_FALLBACK ) {
            uint32_t new = (workers + FOREMAN_THREAD_BUFFER) >= FOREMAN_FALLBACK ? FOREMAN_THREAD_BUFFER : FOREMAN_FALLBACK - workers;
            uint32_t x;
            for (x = 0; x < new; x++) {
                pthread_t* t = malloc(sizeof(pthread_t));
                cvector_add(svc->threads, t);
                pthread_create(t, NULL, svc->init, m);
            }
        }
        pthread_mutex_unlock(&(svc->mutex));
    }
    return RUMBLE_RETURN_OKAY;
}

int rumble_module_init(void* master, rumble_module_info* modinfo) {
    modinfo->title = "Foreman module";
    modinfo->description = "Standard module for dynamically managing worker pools.";
    m = master; // grab the master handle for later use.

    // Hook the module to incoming connections on any service.
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, accept_hook);
    rumble_hook_function(master, RUMBLE_HOOK_POP3 + RUMBLE_HOOK_ACCEPT, accept_hook);
    rumble_hook_function(master, RUMBLE_HOOK_IMAP + RUMBLE_HOOK_ACCEPT, accept_hook);
    return EXIT_SUCCESS; // Tell rumble that the module loaded okay.
}
