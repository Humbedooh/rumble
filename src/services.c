/*$I0 */
#include "rumble.h"
#include "comm.h"
extern masterHandle *comm_master_handle;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumbleService *comm_serviceHandleExtern(masterHandle *master, const char *svcName) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleServicePointer    *svcp = 0;
    c_iterator              iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    cforeach((rumbleServicePointer *), svcp, master->services, iter) {
        if (!strcmp(svcName, svcp->svcName)) {
            return (svcp->svc);
        }
    }

    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumbleService *comm_serviceHandle(const char *svcName) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleServicePointer    *svcp = 0;
    c_iterator              iter;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!svcName) return (0);
    cforeach((rumbleServicePointer *), svcp, comm_master_handle->services, iter) {
        if (!strcmp(svcName, svcp->svcName)) {
            return (svcp->svc);
        }
    }

    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int comm_suspendService(rumbleService *svc) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    c_iterator      iter;
    rumbleThread    *thread;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (svc) {
        rumble_debug(NULL, "svc", "Preparing to suspend service (%s) on port %s.", svc->settings.name, svc->settings.port);
        pthread_mutex_lock(&svc->mutex);
        cforeach((rumbleThread *), thread, svc->threads, iter) {
            if (thread->status == 0)
            {
#ifdef RUMBLE_MSC
                pthread_cancel(thread->thread);
                pthread_kill(thread->thread, 0);
#else
                pthread_cancel(thread->thread);
                pthread_kill(thread->thread, 0);
#endif
                cvector_delete(&iter);
                free(thread);
            } else {
                thread->status = -1;
            }
        }

        svc->threads->size = 0;
        svc->enabled = 2;   /* 2 = suspended */
        pthread_mutex_unlock(&svc->mutex);
    }

    rumble_debug(NULL, "svc", "Suspended service (%s) on port %s.", svc->settings.name, svc->settings.port);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int comm_killService(rumbleService *svc) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    c_iterator      iter;
    rumbleThread    *thread;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (svc) {
        pthread_mutex_lock(&svc->mutex);
        cforeach((rumbleThread *), thread, svc->threads, iter)
        {
#ifdef RUMBLE_MSC
            pthread_cancel(thread->thread);
            pthread_kill(thread->thread, 0);
#else
            pthread_cancel(thread->thread);
            pthread_kill(thread->thread, 0);
#endif
            cvector_delete(&iter);
            thread->status = -1;
            free(thread);
        }

        svc->enabled = 0;   /* 2 = suspended */
        if (svc->socket) disconnect(svc->socket);
        svc->socket = 0;
        pthread_mutex_unlock(&svc->mutex);
    }

    rumble_debug(NULL, "svc", "Killed service (%s) on port %s.", svc->settings.name, svc->settings.port);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int comm_resumeService(rumbleService *svc) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    int             x = 0,
                    y = 0;
    rumbleThread    *thread;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (svc) {
        rumble_debug(NULL, "svc", "Preparing to resume service (%s) on port %s.", svc->settings.name, svc->settings.port);
        if (svc->enabled != 2) return (0);
        pthread_mutex_lock(&svc->mutex);
        y = RUMBLE_INITIAL_THREADS - svc->threads->size;
        y = y > (RUMBLE_INITIAL_THREADS || y < 0) ? RUMBLE_INITIAL_THREADS : y;
        for (x = 0; x < y; x++) {
            thread = (rumbleThread *) malloc(sizeof(rumbleThread));
            thread->status = 0;
            thread->svc = svc;
            cvector_add(svc->threads, thread);
            pthread_create(&thread->thread, 0, svc->init, (void *) thread);
        }

        svc->enabled = 1;   /* 1 = enabled */
        pthread_mutex_unlock(&svc->mutex);
    }

    rumble_debug(NULL, "svc", "Resumed service (%s) on port %s.", svc->settings.name, svc->settings.port);
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumbleService *comm_registerService(masterHandle *master, const char *svcName, void * (*init) (void *), const char *port, int threadCount) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumbleService           *svc;
    rumbleServicePointer    *svcp;
    int                     x = 0;
    traffic_entry           *tentry = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    svcp = (rumbleServicePointer *) malloc(sizeof(rumbleServicePointer));
    svc = (rumbleService *) malloc(sizeof(rumbleService));
    svcp->svc = svc;
    svc->master = master;
    memset(svcp->svcName, 0, 1024);
    strncpy(svcp->svcName, svcName, 1023);
    svc->threads = cvector_init();
    svc->handles = dvector_init();
    svc->commands = cvector_init();
    svc->capabilities = cvector_init();
    svc->init = init;
    pthread_mutex_init(&svc->mutex, 0);
    pthread_cond_init(&svc->cond, NULL);
    svc->enabled = 0;
    svc->traffic.received = 0;
    svc->traffic.sent = 0;
    svc->traffic.sessions = 0;
    svc->traffic.rejections = 0;
    svc->settings.port = port;
    svc->settings.name = svcp->svcName;
    svc->settings.threadCount = threadCount ? threadCount : RUMBLE_INITIAL_THREADS;
    svc->settings.stackSize = 1.5 * 1024 * 1024;
    svc->trafficlog = dvector_init();
    for (x = 0; x < 170; x++) {
        tentry = (traffic_entry *) malloc(sizeof(traffic_entry));
        tentry->bytes = 0;
        tentry->hits = 0;
        tentry->when = 0;
        tentry->rejections = 0;
        dvector_add(svc->trafficlog, tentry);
    }

    cvector_add(master->services, svcp);
    rumble_debug(NULL, "svc", "Registered new service (%s) on port %s.", svcName, port ? port : "(null)");
    return (svc);
}

/*
 =======================================================================================================================
    Adds traffic entry to the statistical data pile (and rotates every week)
 =======================================================================================================================
 */
void comm_addEntry(rumbleService *svc, uint32_t bytes, char rejected) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    time_t          now;
    traffic_entry   *entry = 0;
    dvector_element *obj;
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    now = time(NULL) / 3600;
    if (svc) {
        pthread_mutex_lock(&(svc->mutex));
        entry = (traffic_entry *) svc->trafficlog->first->object;
        if (entry->when < now) {
            obj = svc->trafficlog->first;
            obj->prev = svc->trafficlog->last;
            svc->trafficlog->first = svc->trafficlog->last;
            svc->trafficlog->last = svc->trafficlog->last->prev;
            svc->trafficlog->last->next = 0;
            svc->trafficlog->first->prev = 0;
            svc->trafficlog->first->next = obj;
            entry = (traffic_entry *) svc->trafficlog->first->object;
            entry->bytes = 0;
            entry->hits = 0;
            entry->when = now;
            entry->rejections = 0;
        }

        entry->bytes += bytes;
        if (rejected < 100) {

            /* 100 means discard this info, just rotate the log pile */
            entry->hits++;
            entry->rejections += (rejected ? 1 : 0);
        }

        pthread_mutex_unlock(&(svc->mutex));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int comm_setServiceStack(rumbleService *svc, unsigned int stacksize) {
    if (!svc) return (0);
    svc->settings.stackSize = stacksize;
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int comm_startService(rumbleService *svc) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    rumbleThread    *thread;
    int             n;
    pthread_attr_t  attr;
    /*~~~~~~~~~~~~~~~~~~~~*/

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, svc->settings.stackSize);
    if (!svc) return (0);
    if (svc->settings.port) {
        svc->socket = comm_init(svc->master, svc->settings.port);
        if (!svc->socket) {
            return (0);
        }
    }

    for (n = 0; n < svc->settings.threadCount; n++) {
        thread = (rumbleThread *) malloc(sizeof(rumbleThread));
        thread->status = 0;
        thread->svc = svc;
        cvector_add(svc->threads, thread);
        pthread_create(&thread->thread, &attr, svc->init, (void *) thread);
    }

    svc->enabled = 1;
    rumble_debug(NULL, "svc", "Started service (%s) on port %s.", svc->settings.name, svc->settings.port);
    return (1);
}
