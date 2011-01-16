#include "rumble.h"

#if (RUMBLE_DEBUG & RUMBLE_DEBUG_MEMORY)
	#undef malloc
	#undef calloc
	void* xalloc(size_t m) {
		void* x;
		printf("malloc(%u)\n", m);
		x = malloc(m);
		if (!x) printf("malloc failed!\n");
		return x;
	}
	void* yalloc(size_t n, size_t m) {
		void* x;
		printf("calloc(%u,%u)\n", n,m);
		x = calloc(n,m);
		if (!x) printf("calloc failed!\n");
		return x;
	}
	#define malloc xalloc
	#define calloc yalloc
#endif

void rumble_clean_session(sessionHandle* session) {
	address* el;
    rumble_free_address(&session->sender);
    for ( el = (address*) cvector_first(session->recipients); el != NULL; el = (address*) cvector_next(session->recipients)) {
        rumble_free_address(el);
    }
    cvector_flush(session->recipients);
}


char* rumble_copy_mail(masterHandle* m, const char* fid, const char* usr, const char* dmn) {
    const char* path = rumble_config_str(m, "storagefolder");
    char* nfid = (char*) calloc(1,25);
    char* filename = (char*) calloc(1, strlen(path) + 26);
    char* ofilename = (char*) calloc(1, strlen(path) + 26);
	FILE *fp, *ofp;
	pthread_t p = pthread_self();
        void* pp;
#ifdef PTW32_CDECL
        pp = (void*) p.p;
#else
        pp = p;
#endif
	sprintf(nfid, "%x%x%x", (uint32_t) pp, (uint32_t) time(0), (uint32_t) rand());
    sprintf(filename, "%s/%s", path, nfid);
    sprintf(ofilename, "%s/%s", path, fid);
    fp = fopen(filename, "w");
    ofp = fopen(ofilename, "r");
    #ifdef RUMBLE_DEBUG_STORAGE
        printf("Copying %s to file %s...\n", ofilename, filename);
    #endif
    free(filename);
    free(ofilename);
    if ( fp && ofp ) {
        char* now = rumble_mtime();
		void* buffer = (char*) calloc(1,4096);
        fprintf(fp, "Received: from localhost by %s (rumble) for %s@%s with ESMTP id %s; %s\r\n", rumble_config_str(m,"servername"), usr, dmn, nfid, now);
        free(now);
        while (!feof(ofp)) {
            size_t rc = fread(buffer, 1, 4096, ofp);
            if ( rc < 0 ) break;
            if ( !fwrite(buffer, rc, 1, fp)) break;
        }
        fclose(fp);
        fclose(ofp);
        free(buffer);
    }
    else {
        if (fp) fclose(fp);
        if (ofp) fclose(ofp);
        free(nfid);
        return 0;
    }
    return nfid;
}