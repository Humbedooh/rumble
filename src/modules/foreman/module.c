/*
 * File: module.c Author: Humbedooh A simple (but efficient) load balancing module for rumble. Created
 * on January 3, 2011, 8:08
 */
#include "../../rumble.h"
#define FOREMAN_MAX_JOBS        250 /* Maximum amount of "jobs" each worker is allowed before it's destroyed */
#define FOREMAN_MAX_THREADS     750 /* Max number of threads each service is allowed to run at once. */
#define FOREMAN_FALLBACK        10  /* Fall back to a minimum of 10 workers per service when idling */
#define FOREMAN_THREAD_BUFFER   5   /* Create 5 new workers whenever there's a shortage */
ssize_t accept_hook(sessionHandle *session, const char *junk);  /* Prototype */

/*
 =======================================================================================================================
    Standard module initialization function
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *master, rumble_module_info *modinfo) {
    modinfo->title = "Foreman module";
    modinfo->description = "Standard module for dynamically managing worker pools.";
    modinfo->author = "Humbedooh [humbedooh@users.sf.net]";

    /* Hook the module to incoming connections on any service. */
    rumble_hook_function(master, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_ACCEPT, accept_hook);
    rumble_hook_function(master, RUMBLE_HOOK_POP3 + RUMBLE_HOOK_ACCEPT, accept_hook);
    rumble_hook_function(master, RUMBLE_HOOK_IMAP + RUMBLE_HOOK_ACCEPT, accept_hook);
    return (EXIT_SUCCESS);  /* Tell rumble that the module loaded okay. */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
ssize_t accept_hook(sessionHandle *session, const char *junk) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleService   *svc = NULL;
    rumbleThread    *thread;
    uint32_t        workers,
                    busy,
                    idle,
                    New,
                    x;
    /* If this thread is getting old, tell it to die once it's idling. */
    uint32_t        workload = (session->_tflags & 0xFFF00000) >> 20;   /* 0xABC00000 >> 0x00000ABC */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (workload > FOREMAN_MAX_JOBS) session->_tflags |= RUMBLE_THREAD_DIE;

    /* Find out what service we're dealing with here. */
    svc = (rumbleService *) session->_svc;
    if (svc) {
        if (svc->enabled != 1) return (RUMBLE_RETURN_IGNORE);           /* Return immediately if svc isn't running */

        /* Check if there's a shortage of workers. If there is, make some more, if not, just retur */
        pthread_mutex_lock(&(svc->mutex));
        workers = svc->threads->size;   /* Number of threads alive */
        busy = svc->handles->size;      /* Number of threads busy */
        idle = workers - busy;          /* Number of threads idling */
        if ((idle <= 1 || workers < FOREMAN_FALLBACK) && workers < FOREMAN_MAX_THREADS) {

            /*~~~~~~~~~~~~~~~~~*/
            pthread_attr_t  attr;
            /*~~~~~~~~~~~~~~~~~*/

            pthread_attr_init(&attr);
            pthread_attr_setstacksize(&attr, svc->settings.stackSize);
            New = (workers + FOREMAN_THREAD_BUFFER) >= FOREMAN_FALLBACK ? FOREMAN_THREAD_BUFFER : FOREMAN_FALLBACK - workers;
            for (x = 0; x < New; x++) {
                thread = (rumbleThread *) malloc(sizeof(rumbleThread));
                thread->status = 0;
                thread->svc = svc;
                cvector_add(svc->threads, thread);
                pthread_create(&thread->thread, &attr, svc->init, (void *) thread);
            }
        }

        pthread_mutex_unlock(&(svc->mutex));
    }

    return (RUMBLE_RETURN_OKAY);        /* Tell the thread to continue. */
}

/* Done! */
