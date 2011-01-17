#include "rumble.h"
#include "servers.h"
#include "sqlite3.h"
#include "comm.h"

// Main loop
void* rumble_smtp_init(void* m) {
    masterHandle* master = (masterHandle*) m;
	

    // Initialize a session handle and wait for incoming connections.
    sessionHandle session;
    sessionHandle* sessptr = &session;
	ssize_t rc;
	char *cmd, *arg, *line;
	int x = 0;
    sessionHandle* s;
	void* pp,*tp;
	pthread_t p = pthread_self();
    session.recipients = cvector_init();
    session.client = (clientHandle*) malloc(sizeof(clientHandle));
    session.sender.flags = cvector_init();
    session.sender.domain = 0;
    session.sender.user = 0;
    session.sender.raw = 0;
    session.sender._flags = 0;
    session._master = m;
    session._tflags = RUMBLE_THREAD_SMTP; // Identify the thread/session as SMTP

	#if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
		#ifdef PTW32_CDECL
				pp = (void*) p.p;
		#else
				pp = p;
		#endif
		printf("<smtp::threads> Initialized thread %#x\n", pp);
    #endif
	
    while (1) {
        comm_accept(master->smtp.socket, session.client);
        pthread_mutex_lock(&master->smtp.mutex);
        cvector_add(master->smtp.handles, (void*) sessptr);
        pthread_mutex_unlock(&master->smtp.mutex);
        session.flags = 0;
        session._tflags += 0x00100000; // job count ( 0 through 4095)
        
        #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        printf("<debug::comm> Accepted connection from %s on SMTP\n", session.client->addr);
        #endif
        
        // Check for hooks on accept()
        rc = RUMBLE_RETURN_OKAY;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_SMTP );
        if ( rc == RUMBLE_RETURN_OKAY) rumble_comm_send(sessptr, rumble_smtp_reply_code(220)); // Hello!
        
        // Parse incoming commands
        cmd = (char*) malloc(5);
        arg = (char*) malloc(1024);
		if (!cmd || !arg) { fprintf(stderr, "calloc() failed! (this is bad)\n"); exit(1); }
        while ( rc != RUMBLE_RETURN_FAILURE ) {
            memset(cmd, 0, 5);
            memset(arg, 0, 1024);
            line = rumble_comm_read(sessptr);
            if ( !line ) break;
			rc = 500; // default return code is "500 unknown command thing"
            if (sscanf(line, "%4[^\t ]%*[ \t]%1000[^\r\n]", cmd, arg)) {
				rumble_string_upper(cmd);
				if (!strcmp(cmd, "QUIT")) break; // bye!
				else if (!strcmp(cmd, "MAIL")) rc = rumble_server_smtp_mail(master, &session, arg);
				else if (!strcmp(cmd, "RCPT")) rc = rumble_server_smtp_rcpt(master, &session, arg);
				else if (!strcmp(cmd, "HELO")) rc = rumble_server_smtp_helo(master, &session, arg);
				else if (!strcmp(cmd, "EHLO")) rc = rumble_server_smtp_ehlo(master, &session, arg);
				else if (!strcmp(cmd, "NOOP")) rc = rumble_server_smtp_noop(master, &session, arg);
				else if (!strcmp(cmd, "DATA")) rc = rumble_server_smtp_data(master, &session, arg);
				else if (!strcmp(cmd, "VRFY")) rc = rumble_server_smtp_vrfy(master, &session, arg);
				else if (!strcmp(cmd, "RSET")) rc = rumble_server_smtp_rset(master, &session, arg);
				else if (!strcmp(cmd, "AUTH")) rc = rumble_server_smtp_auth(master, &session, arg);
			}
			free(line);
			if ( rc == RUMBLE_RETURN_IGNORE ) continue; // Skip to next line.
			else if ( rc == RUMBLE_RETURN_FAILURE ) break; // Abort!
			else rumble_comm_send(sessptr, rumble_smtp_reply_code(rc)); // Bad command thing.
        }
        // Cleanup
        #if (RUMBLE_DEBUG & RUMBLE_DEBUG_COMM)
        printf("<debug::comm> Closed connection from %s on SMTP\n", session.client->addr);
        #endif
        rumble_comm_send(sessptr, rumble_smtp_reply_code(221)); // bye!
		
        close(session.client->socket);
        free(arg);
        free(cmd);
        rumble_clean_session(sessptr);
        pthread_mutex_lock(&(master->smtp.mutex));
        
        for (s = (sessionHandle*) cvector_first(master->smtp.handles); s != NULL; s = (sessionHandle*) cvector_next(master->smtp.handles)) {
            if (s == sessptr) { cvector_delete(master->smtp.handles); x = 1; break; }
        }
        // Check if we were told to go kill ourself :(
        if ( session._tflags & RUMBLE_THREAD_DIE ) {
            #if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
            printf("<smtp::threads>I (%#x) was told to die :(\n", (uintptr_t) pthread_self());
            #endif
            cvector_element* el = master->smtp.threads->first;
            while ( el != NULL ) {
                pthread_t* t = (pthread_t*) el->object;
                #ifdef PTW32_CDECL
                        pp = (void*) p.p;
                        tp = t->p;
                #else
                        tp = t;
                #endif
				if (tp == pp) { cvector_delete_at(master->smtp.threads, el); break; }
                el = el->next;
            }
            pthread_mutex_unlock(&master->smtp.mutex);
            pthread_exit(0);
        }
        pthread_mutex_unlock(&master->smtp.mutex);
    }
    pthread_exit(0);
    return 0;
}


// Command specific routines
ssize_t rumble_server_smtp_mail(masterHandle* master, sessionHandle* session, const char* argument) {
	char *raw, *user, *domain, *flags;
	ssize_t rc;
	uint32_t max,size;
    // First, check for the right sequence of commands.
    if ( !(session->flags & RUMBLE_SMTP_HAS_HELO) ) return 503; 
    if ( (session->flags & RUMBLE_SMTP_HAS_MAIL) ) return 503;
    raw = (char*) calloc(1,1000);
    user = (char*) calloc(1,128);
    domain = (char*) calloc(1,128); // RFC says 64, but that was before i18n
    flags = (char*) calloc(1,512); // esmtp flags
	if (!raw || !user || !domain || !flags) merror();
    if (sscanf(argument, "%*4c:%1000c", raw)) {
    
		// Try to fetch standard syntax: MAIL FROM: [whatever] <user@domain.tld>
		if (sscanf(raw, "%*256[^<]<%128[^>@\"]@%128[^@\">]> %500c", user, domain, flags) > 1) {
			// Set the current values
			session->sender.raw = raw;
			session->sender.user = user;
			session->sender.domain = domain;
			session->sender._flags = flags;
			rumble_scan_flags(session->sender.flags, flags);
			// Fire events scheduled for pre-processing run
			rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_MAIL);
			if ( rc != RUMBLE_RETURN_OKAY ) { // Something went wrong, let's clean up and return.
				rumble_free_address(&session->sender);
				return rc;
			}
			// Validate address and any following ESMTP flags.
			if ( !strlen(user) || !strlen(domain) ) {
				rumble_free_address(&session->sender);
				return 501; // Syntax error in MAIL FROM parameter
			}
			max = rumble_config_int(master, "messagesizelimit");
			size = atoi(rumble_get_dictionary_value(session->sender.flags, "SIZE"));
			if ( max != 0 && size != 0 && size > max ) {
				rumble_free_address(&session->sender);
				return 552; // message too big.
			}
    
    
			// Fire post-processing hooks.
			rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_MAIL);
			if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    
			session->flags |= RUMBLE_SMTP_HAS_MAIL;
			return 250;
		}
	}
	return 501; // Syntax error in MAIL FROM parameter
}

ssize_t rumble_server_smtp_rcpt(masterHandle* master, sessionHandle* session, const char* argument) {
	char *raw, *user, *domain;
	address *recipient;
	ssize_t rc;
	uint32_t isLocalDomain, isLocalUser;
    // First, check for the right sequence of commands.
    if ( !(session->flags & RUMBLE_SMTP_HAS_MAIL) ) return 503; 
    
    // Allocate stuff and start parsing
    raw = (char*) calloc(1,1000);
    user = (char*) calloc(1,128);
    domain = (char*) calloc(1,128); // RFC says 64, but that was before i18n
	if (!raw || !user || !domain) merror();
    if (sscanf(argument, "%*2c:%1000c", raw)) {
		// Try to fetch standard syntax: MAIL FROM: [whatever] <user@domain.tld>
		if (sscanf(raw, "%*256[^<]<%128[^\">@]@%128[^@\">]", user, domain) == 2) {
			// Set the current values
			recipient = (address*) malloc(sizeof(address));
			if (!recipient) merror();
			recipient->domain = domain;
			recipient->user = user;
			recipient->raw = raw;
			recipient->_flags = 0;
			recipient->flags = cvector_init();
			cvector_add(session->recipients, recipient);
    
    
			// Fire events scheduled for pre-processing run
			rc = rumble_server_schedule_hooks(master,session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RCPT);
			if ( rc != RUMBLE_RETURN_OKAY ) {
				cvector_pop(session->recipients); // pop the last element from the vector
				rumble_free_address(recipient); // flush the memory
				return rc;
			}
            
			// Check if recipient is local
			isLocalDomain = rumble_domain_exists(session, domain);
			isLocalUser = isLocalDomain ? rumble_account_exists(session, user, domain) : 0;
    
			if ( isLocalUser )     {
			// If everything went fine, set the RCPT flag and return with code 250.
				// >>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
				// >>>>>>>>>>>>>>>>>>>>>> !!! TODO !!! <<<<<<<<<<<<<<<<<<<<<<<
				// Check if user has space in mailbox for this msg!
				// >>>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
				session->flags |= RUMBLE_SMTP_HAS_RCPT;
				return 250;
			}
			// If rec isn't local, check if client is allowed to relay
			if ( !isLocalDomain ) {
				if ( session->flags & RUMBLE_SMTP_CAN_RELAY ) {
					session->flags |= RUMBLE_SMTP_HAS_RCPT;
					return 251;
				}
				// Not local and no relaying allowed, return 530.
				cvector_pop(session->recipients);
				rumble_free_address(recipient);
				return 530;
			}
			// Domain is local but user doesn't exist, return 550
			cvector_pop(session->recipients);
			rumble_free_address(recipient);
			return 550;
		}
	}
	return 501; // Syntax error in RCPT TO parameter
}

ssize_t rumble_server_smtp_helo(masterHandle* master, sessionHandle* session, const char* argument) {
    session->flags |= RUMBLE_SMTP_HAS_HELO;
    rumble_add_dictionary_value(session->sender.flags, "helo", argument);
    return 250;
}

ssize_t rumble_server_smtp_ehlo(masterHandle* master, sessionHandle* session, const char* argument) {
    session->flags |= RUMBLE_SMTP_HAS_EHLO;
    rumble_add_dictionary_value(session->sender.flags, "helo", argument);
    rumble_comm_send(session, "250-Extended commands follow\r\n");
    rumble_comm_send(session, "250-EXPN\r\n");
    rumble_comm_send(session, "250-VRFY\r\n");
    rumble_comm_send(session, "250-PIPELINING\r\n");
    rumble_comm_send(session, "250-8BITMIME\r\n");
    rumble_comm_send(session, "250-AUTH CRAM-MD5 DIGEST-MD5 PLAIN\r\n");
    rumble_comm_send(session, "250-DELIVERBY 900\r\n");
    rumble_comm_send(session, "250-DSN\r\n");
    rumble_comm_send(session, "250-SIZE\r\n");
    rumble_comm_send(session, "250 XVERP\r\n");
    return RUMBLE_RETURN_IGNORE;
}

ssize_t rumble_server_smtp_data(masterHandle* master, sessionHandle* session, const char* argument) {
	char *fid, *filename, *log, *now, *line;
	const char *sf;
	FILE* fp;
	address* el;
	pthread_t p = pthread_self();
        void* pp;
#ifdef PTW32_CDECL
        pp = (void*) p.p;
#else
        pp = p;
#endif
	
    // First, check for the right sequence of commands.
    if ( !(session->flags & RUMBLE_SMTP_HAS_RCPT) ) return 503;
    
    // Make a unique filename and try to open the storage folder for writing.
    fid = (char*) calloc(1,25);
	if (!fid) merror();
	sprintf(fid, "%x%x%x", (uint32_t) pp, (uint32_t) time(0), (uint32_t) rand());
    sf = rumble_config_str(master, "storagefolder");
    filename = (char*) calloc(1, strlen(sf) + 26);
	if (!filename) merror();
    sprintf(filename, "%s/%s", sf, fid);
    fp = fopen(filename, "wb");
#ifdef RUMBLE_DEBUG_STORAGE
    printf("Writing to file %s...\n", filename);
#endif
    free(filename);
    if ( !fp ) {
#ifdef RUMBLE_DEBUG_STORAGE
        perror("Couldn't open file for writing");
#endif
        free(fid);
        return 451; // Couldn't open file for writing :/
    }
    log = (char*) calloc(1,1024);
	if (!log) merror();
    now = rumble_mtime();
    sprintf(log, "Received: from %s <%s> by %s (rumble) with ESMTP id %s; %s\r\n", rumble_get_dictionary_value(session->sender.flags, "helo"), session->client->addr, rumble_config_str(master, "servername"), fid, now);
    free(now);
    fwrite(log, strlen(log), 1, fp);
    rumble_comm_send(session, rumble_smtp_reply_code(354));
    
    // Save the message
    while ( 1 ) {
        line = rumble_comm_read(session);
        // Check for broken connection
        if ( !line ) {
            fclose(fp);
            free(fid);
            return RUMBLE_RETURN_FAILURE;
        }
        if ( !strcmp(line, ".\r\n") ) break; // We're done here.
        if ( fwrite(line, strlen(line), 1, fp) != 1 ) { // Writing failed?
            fclose(fp);
            free(fid);
            return 452;
        }
    }
    fclose(fp);

    for ( el = (address*) cvector_first(session->recipients); el != NULL; el = (address*) cvector_next(session->recipients)) {
        sqlite3_stmt* state = rumble_sql_inject((sqlite3*) master->readOnly.db, \
                "INSERT INTO queue (fid, sender, user, domain, flags) VALUES (?,?,?,?,?)", \
                fid, session->sender.raw, el->user, el->domain, session->sender._flags);
        /*int rc = */sqlite3_step(state);
        sqlite3_finalize(state);
    }
    free(fid);
    return 250;
}

ssize_t rumble_server_smtp_rset(masterHandle* master, sessionHandle* session, const char* argument) {
    ssize_t rc;
    // Fire events scheduled for pre-processing run
    rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RSET);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    
    // Reset the session handle
    session->flags = 0;
    rumble_clean_session(session);
    
    // Fire post-processing hooks.
    rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RSET);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    return 250;
}

ssize_t rumble_server_smtp_vrfy(masterHandle* master, sessionHandle* session, const char* argument) {
	ssize_t rc;
    char* user = (char*) calloc(1,128);
    char* domain = (char*) calloc(1, 128);
    if (sscanf(argument, "%128[^@\"]@%128[^\"]", user, domain) ) {
		// Fire events scheduled for pre-processing run
		rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_VRFY);
		if ( rc != RUMBLE_RETURN_OKAY ) return rc;
		// Check if account exists
		return rumble_account_exists(session,user,domain) ? 250 : 550;
	}
	return 501;
}

ssize_t rumble_server_smtp_noop(masterHandle* master, sessionHandle* session, const char* argument) {
    ssize_t rc;
    // Fire events scheduled for pre-processing run
    rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_NOOP);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    // Do...nothing    
    // Fire post-processing hooks.
    rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_NOOP);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    return 250;
}

ssize_t rumble_server_smtp_auth(masterHandle* master, sessionHandle* session, const char* argument) {
    return 250;
}