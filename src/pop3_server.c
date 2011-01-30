#include "rumble.h"
#include "servers.h"
#include "comm.h"

// Main loop
void* rumble_pop3_init(void* m) {
    masterHandle* master = (masterHandle*) m;
	

    // Initialize a session handle and wait for incoming connections.
    sessionHandle session;
    sessionHandle* sessptr = &session;
	ssize_t rc;
	char *cmd, *arg, *line, *tmp;
	const char* myName;
	int x = 0;
	time_t now;
    sessionHandle* s;
	pop3Session* pops;
	void* pp,*tp;
	pthread_t p = pthread_self();
	session.dict = cvector_init();
    session.recipients = cvector_init();
	session._svcHandle = (pop3Session*) malloc(sizeof(pop3Session));
    session.client = (clientHandle*) malloc(sizeof(clientHandle));
    session._master = m;
	pops = (pop3Session*) session._svcHandle;
	pops->account = 0;
	pops->bag = 0;
    session._tflags = RUMBLE_THREAD_POP3; // Identify the thread/session as POP3
	myName = rrdict(master->_core.conf, "servername");
	myName = myName ? myName : "??";
	tmp = (char*) malloc(100);
	#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
		#ifdef PTW32_CDECL
				pp = (void*) p.p;
		#else
				pp = p;
		#endif
		printf("<pop3::threads> Initialized thread %#x\n", pp);
    #endif
	
    while (1) {
        comm_accept(master->pop3.socket, session.client);
        pthread_mutex_lock(&master->pop3.mutex);
        cvector_add(master->pop3.handles, (void*) sessptr);
        pthread_mutex_unlock(&master->pop3.mutex);
        session.flags = 0;
        session._tflags += 0x00100000; // job count ( 0 through 4095)
        session.sender = 0;
		now = time(0);
        
        #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
		strftime(tmp, 100, "%X", localtime(&now));
        printf("<debug::comm> [%s] Accepted connection from %s on POP3\n", tmp, session.client->addr);
        #endif
        
        // Check for hooks on accept()
        rc = RUMBLE_RETURN_OKAY;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_POP3 );
		if ( rc == RUMBLE_RETURN_OKAY) rcprintf(sessptr, rumble_pop3_reply_code(101), myName); // Hello!
        
        // Parse incoming commands
        cmd = (char*) malloc(5);
        arg = (char*) malloc(1024);
		if (!cmd || !arg) merror();
        while ( rc != RUMBLE_RETURN_FAILURE ) {
            memset(cmd, 0, 5);
            memset(arg, 0, 1024);
            line = rumble_comm_read(sessptr);
			rc = 421;
            if ( !line ) break;
			rc = 105; // default return code is "500 unknown command thing"
            if (sscanf(line, "%4[^\t ]%*[ \t]%1000[^\r\n]", cmd, arg)) {
				rumble_string_upper(cmd);
				if (!strcmp(cmd, "QUIT")) break; // bye!
				else if (!strcmp(cmd, "CAPA")) rc = rumble_server_pop3_capa(master, &session, arg);
				else if (!strcmp(cmd, "USER")) rc = rumble_server_pop3_user(master, &session, arg);
				else if (!strcmp(cmd, "PASS")) rc = rumble_server_pop3_pass(master, &session, arg);
				else if (!strcmp(cmd, "TOP"))  rc = rumble_server_pop3_top(master, &session, arg);
				else if (!strcmp(cmd, "UIDL")) rc = rumble_server_pop3_uidl(master, &session, arg);
				else if (!strcmp(cmd, "DELE")) rc = rumble_server_pop3_dele(master, &session, arg);
				else if (!strcmp(cmd, "RETR")) rc = rumble_server_pop3_retr(master, &session, arg);
				else if (!strcmp(cmd, "LIST")) rc = rumble_server_pop3_list(master, &session, arg);
			}
			free(line);
			if ( rc == RUMBLE_RETURN_IGNORE ) continue; // Skip to next line.
			else if ( rc == RUMBLE_RETURN_FAILURE ) break; // Abort!
			else rumble_comm_send(sessptr, rumble_pop3_reply_code(rc)); // Bad command thing.
        }
        // Cleanup
        #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
		now = time(0);
        strftime(tmp, 100, "%X", localtime(&now));
        printf("<debug::comm> [%s] Closing connection from %s on POP3\n", tmp, session.client->addr);
        #endif
		if ( rc == 421 ) rumble_comm_send(sessptr, rumble_pop3_reply_code(103)); // timeout!
        else rumble_comm_send(sessptr, rumble_pop3_reply_code(102)); // bye!
		
        close(session.client->socket);

		/* Start cleanup */
        free(arg);
        free(cmd);
        rumble_clean_session(sessptr);
		rumble_letters_purge(pops->bag); /* delete any letters marked for deletion */
		rumble_letters_flush(pops->bag); /* flush the mail bag from memory */
		if ( pops->account ) rumble_free_account(pops->account);
		/* End cleanup */

        pthread_mutex_lock(&(master->pop3.mutex));
        
        for (s = (sessionHandle*) cvector_first(master->pop3.handles); s != NULL; s = (sessionHandle*) cvector_next(master->pop3.handles)) {
            if (s == sessptr) { cvector_delete(master->pop3.handles); x = 1; break; }
        }
        // Check if we were told to go kill ourself :(
        if ( session._tflags & RUMBLE_THREAD_DIE ) {
            #if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
            printf("<pop3::threads>I (%#x) was told to die :(\n", (uintptr_t) pthread_self());
            #endif
            cvector_element* el = master->pop3.threads->first;
            while ( el != NULL ) {
                pthread_t* t = (pthread_t*) el->object;
                #ifdef PTW32_CDECL
                        pp = (void*) p.p;
                        tp = t->p;
                #else
                        tp = t;
                        pp = p;
                #endif
				if (tp == pp) { cvector_delete_at(master->pop3.threads, el); break; }
                el = el->next;
            }
            pthread_mutex_unlock(&master->pop3.mutex);
            pthread_exit(0);
        }
        pthread_mutex_unlock(&master->pop3.mutex);
    }
    pthread_exit(0);
    return 0;
}

ssize_t rumble_server_pop3_capa(masterHandle* master, sessionHandle* session, const char* argument) {
	rcsend(session, "+OK Here's what I got:\r\n");
	rcsend(session, "TOP\r\n");
	rcsend(session, "UIDL\r\n");
	rcsend(session, "USER\r\n");
	rcsend(session, "PIPELINING\r\n");
	rcsend(session, "IMPLEMENTATION Rumble Mail Server\r\n");
	rcsend(session, ".\r\n");
	return RUMBLE_RETURN_IGNORE;
}

ssize_t rumble_server_pop3_user(masterHandle* master, sessionHandle* session, const char* argument) {
	if ( session->flags & RUMBLE_POP3_HAS_AUTH ) return 105;
	rfdict(session->dict);
	rsdict(session->dict, "user", argument);
	session->flags |= RUMBLE_POP3_HAS_USER;
	return 104;
}

ssize_t rumble_server_pop3_pass(masterHandle* master, sessionHandle* session, const char* argument) {
	char *usr, *dmn, *tmp;
	pop3Session* pops = (pop3Session*) session->_svcHandle;
	if ( ! (session->flags & RUMBLE_POP3_HAS_USER) ) return 105;
	if ( session->flags & RUMBLE_POP3_HAS_AUTH ) return 105;
	usr = (char*) calloc(1,128);
	dmn = (char*) calloc(1,128);
	tmp = (char*) calloc(1,65);
	if ( sscanf(rrdict(session->dict, "user"), "%128[^@]@%128c", usr, dmn) == 2 ) {
		printf("searching for user <%s> @ <%s>\n", usr,dmn);
		if ( ( pops->account = rumble_account_data(session, usr, dmn)) ) {
			tmp = rumble_sha256((const unsigned char*) argument);
			printf("matching %s against %s\n", tmp, pops->account->hash);
			if ( strcmp(tmp, pops->account->hash) ) {
				free(usr); free(dmn); free(tmp);
				rumble_free_account(pops->account);
				free(pops->account);
				pops->account = 0;
				return 106;
			}
			free(usr); free(dmn); free(tmp);
			session->flags |= RUMBLE_POP3_HAS_AUTH;
			pops->bag = rumble_letters_retreive(pops->account);
			return 104;
		}
	}
	return 106; // bad user/pass given.
}

ssize_t rumble_server_pop3_list(masterHandle* master, sessionHandle* session, const char* argument) {
	rumble_letter* letter;
	uint32_t i;
	pop3Session* pops = (pop3Session*) session->_svcHandle;
	if ( ! (session->flags & RUMBLE_POP3_HAS_AUTH) ) return 105; // Not authed?! :(
	rcsend(session, "+OK\r\n");
	for ( i = 0; i < pops->bag->size; i++ ) {
		letter = pops->bag->letters[i];
		if (! (letter->flags & RUMBLE_LETTER_DELETED) ) rcprintf(session, "%u %u\r\n", i+1, letter->size);
	}
	rcsend(session, ".\r\n");
	return RUMBLE_RETURN_IGNORE;
}

ssize_t rumble_server_pop3_uidl(masterHandle* master, sessionHandle* session, const char* argument) {
	rumble_letter* letter;
	uint32_t i;
	pop3Session* pops = (pop3Session*) session->_svcHandle;
	if ( ! (session->flags & RUMBLE_POP3_HAS_AUTH) ) return 105; // Not authed?! :(
	rcsend(session, "+OK\r\n");
	for ( i = 0; i < pops->bag->size; i++ ) {
		letter = pops->bag->letters[i];
		if (! (letter->flags & RUMBLE_LETTER_DELETED) ) rcprintf(session, "%u %s\r\n", i+1, letter->fid);
	}
	rcsend(session, ".\r\n");
	return RUMBLE_RETURN_IGNORE;
}

ssize_t rumble_server_pop3_dele(masterHandle* master, sessionHandle* session, const char* argument) {
	rumble_letter* letter;
	char* tmp;
	void* state;
	int i, found;
	pop3Session* pops = (pop3Session*) session->_svcHandle;
	if ( ! (session->flags & RUMBLE_POP3_HAS_AUTH) ) return 105; // Not authed?! :(
	i = atoi(argument);
	found = 0;
	if ( i <= pops->bag->size && i > 0 ) {
		letter = pops->bag->letters[i-1];
		letter->flags |= RUMBLE_LETTER_DELETENOW;
		found = 1;
	}
	if ( found ) rcsend(session, "+OK\r\n");
	else rcsend(session, "-ERR No such letter.\r\n");
	return RUMBLE_RETURN_IGNORE;
}


ssize_t rumble_server_pop3_retr(masterHandle* master, sessionHandle* session, const char* argument) {
	rumble_letter* letter;
	char buffer[2049];
	FILE* fp;
	void* state;
	int i, found;
	pop3Session* pops = (pop3Session*) session->_svcHandle;
	if ( ! (session->flags & RUMBLE_POP3_HAS_AUTH) ) return 105; // Not authed?! :(
	i = atoi(argument);
	found = 0;
	if ( i <= pops->bag->size && i > 0 ) {
		letter = pops->bag->letters[i-1];
		found = 1;
		fp = rumble_letters_open(letter);
		if (fp) {
			rcsend(session, "+OK\r\n");
			while (!feof(fp)) {
				fgets(buffer, 2048, fp);
				rcsend(session, buffer);
			}
			fclose(fp);
			rcsend(session, ".\r\n");
		}
		else {
			rcsend(session, "-ERR Internal I/O error while accessing mail file.\r\n");
		}
	}
	if (!found) rcsend(session, "-ERR No such letter.\r\n");
	return RUMBLE_RETURN_IGNORE;
}

ssize_t rumble_server_pop3_top(masterHandle* master, sessionHandle* session, const char* argument) {
	char *tmp, buffer[2049];
	FILE* fp;
	void* state;
	int i, found, lines;
	pop3Session* pops = (pop3Session*) session->_svcHandle;
	if ( ! (session->flags & RUMBLE_POP3_HAS_AUTH) ) return 105; // Not authed?! :(
	if ( sscanf(argument, "%i %i", &i, &lines) == 2 ) {
		found = 0;
		if ( i > 0 && i <= pops->bag->size ) {
			fp = rumble_letters_open(pops->bag->letters[i-1]);
			if (fp) {
				rcsend(session, "+OK\r\n");
				while (!feof(fp) && lines) {
					lines--;
					fgets(buffer, 2048, fp);
					rcsend(session, buffer);
				}
				fclose(fp);
				rcsend(session, ".\r\n");
			}
			else {
				rcsend(session, "-ERR Internal I/O error while accessing mail file.\r\n");
			}
			return RUMBLE_RETURN_IGNORE;
		}
		if (!found) rcsend(session, "-ERR No such letter.\r\n");
		return RUMBLE_RETURN_IGNORE;
	}
	return 105;
}