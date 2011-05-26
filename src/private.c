/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_MEMORY)
#   undef malloc
#   undef calloc

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *xalloc(size_t m) {

    /*~~~~~~~*/
    void    *x;
    /*~~~~~~~*/

    printf("malloc(%u)\n", m);
    x = malloc(m);
    if (!x) printf("malloc failed!\n");
    return (x);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *yalloc(size_t n, size_t m) {

    /*~~~~~~~*/
    void    *x;
    /*~~~~~~~*/

    printf("calloc(%u,%u)\n", n, m);
    x = calloc(n, m);
    if (!x) printf("calloc failed!\n");
    return (x);
}

#   define malloc  xalloc
#   define calloc  yalloc
#endif

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_clean_session(sessionHandle *session) {

    /*~~~~~~~~~~~~~*/
    address     *el;
    d_iterator  iter;
    /*~~~~~~~~~~~~~*/

    if (!session) return;
    rumble_free_address(session->sender);
    foreach((address *), el, session->recipients, iter) {
        rumble_free_address(el);
    }

    dvector_flush(session->recipients);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
size_t rumble_copy_mail(masterHandle *m, const char *fid, const char *usr, const char *dmn, char **pfid) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char  *path = rumble_config_str(m, "storagefolder");
    char        *nfid;
    char        *filename = (char *) calloc(1, strlen(path) + 26);
    char        *ofilename = (char *) calloc(1, strlen(path) + 26);
    FILE        *fp,
                *ofp;
    size_t      fsize = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!filename || !ofilename) merror();
    nfid = rumble_create_filename();
    sprintf(filename, "%s/%s", path, nfid);
    sprintf(ofilename, "%s/%s", path, fid);
    fp = fopen(filename, "wb");
    ofp = fopen(ofilename, "r");
#ifdef RUMBLE_DEBUG_STORAGE
    printf("Copying %s to file %s...\n", ofilename, filename);
#endif
    free(filename);
    free(ofilename);
    if (!fp || !ofp) {
        perror("Couldn't copy file");
        if (fp) fclose(fp);
        if (ofp) fclose(ofp);
        free(nfid);
        *pfid = 0;
        return (0);
    } else {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        char    *now = rumble_mtime();
        void    *buffer = (char *) calloc(1, 4096);
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if (!now || !buffer) merror();
        fprintf(fp, "Received: from localhost by %s (rumble) for %s@%s with ESMTPA id <%s>; %s\r\n", rumble_config_str(m, "servername"),
                usr, dmn, nfid, now);
        free(now);
        while (!feof(ofp)) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            size_t  rc = fread(buffer, 1, 4096, ofp);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            if (rc < 0) break;
            if (!fwrite(buffer, rc, 1, fp)) break;
            fsize += rc;
        }

        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        rewind(fp);
        fclose(fp);
        fclose(ofp);
        free(buffer);
        *pfid = nfid;
    }

    return (fsize);
}

/*
 =======================================================================================================================
    rumble_readerwriter: A simple reader/writer mechanism that allows multiple readers to access the same memory, but
    grants exclusive access whenever a writer requests write access.
 =======================================================================================================================
 */
rumble_readerwriter *rumble_rw_init(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    rumble_readerwriter *rrw = (rumble_readerwriter *) malloc(sizeof(rumble_readerwriter));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!rrw) merror();
    rrw->readers = 0;
    rrw->writers = 0;
    pthread_mutex_init(&rrw->mutex, 0);
    pthread_cond_init(&rrw->reading, 0);
    pthread_cond_init(&rrw->writing, 0);
    return (rrw);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_rw_start_read(rumble_readerwriter *rrw)
{
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS)
    printf("rrw: <read>\n");
#endif
    pthread_mutex_lock(&rrw->mutex);

    /* Wait for any writers working (or queued for work) to do their stuff. */
    while (rrw->writers) {
        pthread_cond_wait(&rrw->writing, &rrw->mutex);
    }

    /* Announce that we're reading now. */
    rrw->readers++;
    pthread_mutex_unlock(&rrw->mutex);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_rw_stop_read(rumble_readerwriter *rrw)
{
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS)
    printf("rrw: </read>\n");
#endif
    pthread_mutex_lock(&rrw->mutex);
    rrw->readers--;

    /*
     * If a writer is waiting;
     * Signal that we've stopped reading
     */
    if (rrw->writers) pthread_cond_broadcast(&rrw->reading);
    pthread_mutex_unlock(&rrw->mutex);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_rw_start_write(rumble_readerwriter *rrw)
{
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS)
    printf("rrw: <write>\n");
#endif
    pthread_mutex_lock(&rrw->mutex);

    /* Wait for any previous writer to finish */
    while (rrw->writers) {
        pthread_cond_wait(&rrw->writing, &rrw->mutex);
    }

    /* Let readers know that we want to write */
    rrw->writers++;

    /* Wait for all readers to quit */
    while (rrw->readers) {
        pthread_cond_wait(&rrw->reading, &rrw->mutex);
    }

    pthread_mutex_unlock(&rrw->mutex);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_rw_stop_write(rumble_readerwriter *rrw)
{
#if (RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS)
    printf("rrw: </write>\n");
#endif
    pthread_mutex_lock(&rrw->mutex);
    if (rrw->writers) {
        rrw->writers--;
    }

    /* Signal that we've stopped writing now */
    pthread_cond_broadcast(&rrw->writing);
    pthread_mutex_unlock(&rrw->mutex);
}
