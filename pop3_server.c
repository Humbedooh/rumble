#include "rumble.h"
#include "servers.h"

void* rumble_pop3_init(void* m) {
    masterHandle* master = (masterHandle*) m;
    printf(".");
    while (1) {
        // Initialize a session handle and wait for incoming connections.
        sessionHandle session;
        session.client = (clientHandle*) comm_accept(master->pop3.socket);
        session.flags = 0;
        session.recipients = cvector_init();
        
        // Check for hooks on accept()
        ssize_t rc = EXIT_SUCCESS;
        rc = rumble_server_schedule_hooks(master, &session, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_POP3 );
        rumble_comm_send(&session, rumble_pop3_reply_code(220)); // Hello!
        
        // Parse incoming commands
        char* cmd = malloc(5);
        char* arg = malloc(1024);
        while ( rc != -1 ) {
            memset(cmd, 0, 5);
            memset(arg, 0, 1024);
            char* line = rumble_comm_read(&session);
            if ( !line ) break;
            sscanf(line, "%4[^\t ]%*[ \t]%1000c", cmd, arg);
            free(line);
            rumble_string_upper(cmd);
            rc = 500; // default return code is "500 lolwut?"
            if (!strcmp(cmd, "QUIT")) break; // bye!
            else if (!strcmp(cmd, "USER")) rc = rumble_server_smtp_mail(master, &session, arg);
            if ( rc == RUMBLE_RETURN_IGNORE ) continue; // Skip to next line.
            else if ( rc == RUMBLE_RETURN_FAILURE ) break; // Abort!
            else rumble_comm_send(&session, rumble_pop3_reply_code(rc)); // Bad mojo!
        }
        // Cleanup
        rumble_comm_send(&session, rumble_pop3_reply_code(221)); // bye!
        close(session.client->socket);
        free(arg);
        free(cmd);
    }
    pthread_exit(0);
}
