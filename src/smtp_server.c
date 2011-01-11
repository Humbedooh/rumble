#include "rumble.h"
#include "servers.h"
#include "comm.h"


// Main loop
void* rumble_smtp_init(void* m) {
    masterHandle* master = (masterHandle*) m;
    #if RUMBLE_DEBUG & RUMBLE_DEBUG_THREADS
    printf("<smtp::threads> Initialized thread %#x\n", (uintptr_t) pthread_self());
    #endif
    
    // Initialize a session handle and wait for incoming connections.
    sessionHandle session;
    sessionHandle* sessptr = &session;
    session.recipients = cvector_init();
    session.client = (clientHandle*) malloc(sizeof(clientHandle));
    session.sender.flags = cvector_init();
    session.sender.domain = 0;
    session.sender.user = 0;
    session.sender.raw = 0;
    session._master = m;
    session._tflags = RUMBLE_THREAD_SMTP; // Identify the thread/session as SMTP
    while (1) {
        comm_accept(master->smtp.socket, session.client);
        pthread_mutex_lock(&master->smtp.mutex);
        cvector_add(master->smtp.handles, (void*) sessptr);
        pthread_mutex_unlock(&master->smtp.mutex);
        session.flags = 0;
        session._tflags += 0x00100000; // job count ( 0 through 4095)
        
        // Check for hooks on accept()
        ssize_t rc = EXIT_SUCCESS;
        rc = rumble_server_schedule_hooks(master, sessptr, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_SMTP );
        rumble_comm_send(sessptr, rumble_smtp_reply_code(220)); // Hello!
        
        // Parse incoming commands
        char* cmd = malloc(5);
        char* arg = malloc(1024);
        while ( rc != -1 ) {
            memset(cmd, 0, 5);
            memset(arg, 0, 1024);
            char* line = rumble_comm_read(sessptr);
            if ( !line ) break;
            sscanf(line, "%4[^\t ]%*[ \t]%1000[^\r\n]", cmd, arg);
            free(line);
            rumble_string_upper(cmd);
            rc = 500; // default return code is "500 lolwut?"
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
            if ( rc == RUMBLE_RETURN_IGNORE ) continue; // Skip to next line.
            else if ( rc == RUMBLE_RETURN_FAILURE ) break; // Abort!
            else rumble_comm_send(sessptr, rumble_smtp_reply_code(rc)); // Bad command thing.
        }
        // Cleanup
        rumble_comm_send(sessptr, rumble_smtp_reply_code(221)); // bye!
        shutdown(session.client->socket,0);
        close(session.client->socket);
        free(arg);
        free(cmd);
        rumble_clean_session(sessptr);
        pthread_mutex_lock(&(master->smtp.mutex));
        int x = 0;
        sessionHandle* s;
        for (s = (sessionHandle*) cvector_first(master->smtp.handles); s != NULL; s = cvector_next(master->smtp.handles)) {
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
                if (*t == pthread_self()) { cvector_delete_at(master->smtp.threads, el); break; }
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
    // First, check for the right sequence of commands.
    if ( !(session->flags & RUMBLE_SMTP_HAS_HELO) ) return 503; 
    if ( (session->flags & RUMBLE_SMTP_HAS_MAIL) ) return 503; 
    
    char* raw = calloc(1,1000);
    char* user = calloc(1,128);
    char* domain = calloc(1,128); // RFC says 64, but that was before i18n
    char* flags = calloc(1,512); // esmtp flags
    sscanf(argument, "%*4c:%1000c", raw);
    // Try to fetch standard syntax: MAIL FROM: [whatever] <user@domain.tld>
    sscanf(raw, "%*256[^<]<%128[^>@\"]@%128[^@\">] %500c", user, domain, flags);
    // Set the current values
    session->sender.raw = raw;
    session->sender.user = user;
    session->sender.domain = domain;
    rumble_scan_flags(session->sender.flags, flags);
    // Fire events scheduled for pre-processing run
    ssize_t rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_MAIL);
    if ( rc != RUMBLE_RETURN_OKAY ) { // Something went wrong, let's clean up and return.
        rumble_free_address(&session->sender);
        session->flags = session->flags ^ RUMBLE_SMTP_HAS_MAIL;
        return rc;
    }
    // Validate address and any following ESMTP flags.
    uint32_t max = rumble_config_int("messagesizelimit");
    uint32_t size = atoi(rumble_get_dictionary_value(session->sender.flags, "SIZE"));
    if ( max != 0 && size != 0 && size > max ) {
        rumble_free_address(&session->sender);
        session->flags = session->flags ^ RUMBLE_SMTP_HAS_MAIL;
        return 552; // message too big.
    }
    
    
    // Fire post-processing hooks.
    rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_MAIL);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    
    session->flags = session->flags | RUMBLE_SMTP_HAS_MAIL;
    return 250;
}

ssize_t rumble_server_smtp_rcpt(masterHandle* master, sessionHandle* session, const char* argument) {
    // First, check for the right sequence of commands.
    if ( !(session->flags & RUMBLE_SMTP_HAS_MAIL) ) return 503; 
    
    // Allocate stuff and start parsing
    char* raw = calloc(1,1000);
    char* user = calloc(1,128);
    char* domain = calloc(1,128); // RFC says 64, but that was before i18n
    sscanf(argument, "%*2c:%1000c", raw);
    // Try to fetch standard syntax: MAIL FROM: [whatever] <user@domain.tld>
    sscanf(raw, "%*256[^<]<%128[^\">@]@%128[^@\">]", user, domain);
    // Set the current values
    address* recipient = malloc(sizeof(address));
    recipient->domain = domain;
    recipient->user = user;
    recipient->raw = raw;
    recipient->flags = cvector_init();
    cvector_add(session->recipients, recipient);
    
    ssize_t rc;
    // Fire events scheduled for pre-processing run
    rc = rumble_server_schedule_hooks(master,session, RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_RCPT);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    
    
    // Check if recipient is local
    uint32_t isLocalDomain = rumble_domain_exists(session, recipient->domain);
    uint32_t isLocalUser = isLocalDomain ? rumble_account_exists(session,recipient->user, recipient->domain) : 0;
    
    if ( isLocalUser )     {
    // If everything went fine, set the RCPT flag and return with code 250.
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
        return 530;
    }
    // Domain is local but user doesn't exist, return 550
    return 550;
}

ssize_t rumble_server_smtp_helo(masterHandle* master, sessionHandle* session, const char* argument) {
    session->flags = session->flags | RUMBLE_SMTP_HAS_HELO;
    return 250;
}

ssize_t rumble_server_smtp_ehlo(masterHandle* master, sessionHandle* session, const char* argument) {
    session->flags = session->flags | RUMBLE_SMTP_HAS_EHLO;
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
    // First, check for the right sequence of commands.
    if ( !(session->flags & RUMBLE_SMTP_HAS_RCPT) ) return 503;
    
    // Make a unique filename and try to open the storage folder for writing.
    char* fid = calloc(1,25);
    sprintf(fid, "%x%x%x", (uint32_t) pthread_self(), (uint32_t) time(0), (uint32_t) rand());
    const char* sf = rumble_config_str("storagefolder");
    char* filename = calloc(1, strlen(sf) + 26);
    sprintf(filename, "%s/%s", sf, fid);
    FILE* fp = fopen(filename, "w");
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
    rumble_comm_send(session, rumble_smtp_reply_code(354));
    char* line;
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
    char* user = calloc(1,128);
    char* domain = calloc(1, 128);
    sscanf(argument, "%128[^@\"]@%128[^\"]", user, domain);
    ssize_t rc;
    // Fire events scheduled for pre-processing run
    rc = rumble_server_schedule_hooks(master,session,RUMBLE_HOOK_SMTP + RUMBLE_HOOK_COMMAND + RUMBLE_HOOK_BEFORE + RUMBLE_CUE_SMTP_VRFY);
    if ( rc != RUMBLE_RETURN_OKAY ) return rc;
    // Check if account exists
    return rumble_account_exists(session,user,domain) ? 250 : 550;
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