#include "rumble.h"

void* rumble_smtp_init(void* m) {
    masterHandle* master = (masterHandle*) m;
    printf(".");
    while (1) {
        // Initialize a session handle and wait for incoming connections.
        sessionHandle session;
        session.client = (clientHandle*) comm_accept(master->smtp.socket);
        session.flags = 0;
        
        // Check for hooks on accept()
        int g = 0;
        ssize_t rc = EXIT_SUCCESS;
        if ( cvector_size(master->smtp.init_hooks)) {
            hookHandle* el;
            for (el = (hookHandle*) cvector_first(master->smtp.init_hooks); el != NULL; el = (hookHandle*) cvector_next(master->smtp.init_hooks)) {
                g++;
                ssize_t (*init)(sessionHandle*) = el->func;
                rc = (*init)(&session);
            }
        }
        
        rumble_comm_send(&session, rumble_smtp_reply_code(220)); // Hello!
        
        // Parse incoming commands
        char* cmd = malloc(5);
        char* arg = malloc(1024);
        while ( rc != 1 ) {
            memset(cmd, 0, 5);
            memset(arg, 0, 1024);
            char* line = rumble_comm_read(&session);
            sscanf(line, "%4[^\t ]%*[ \t]%1000c", cmd, arg);
            free(line);
            rumble_string_upper(cmd);
            if ( !strcmp(cmd, "QUIT") ) {
                rumble_comm_send(&session, rumble_smtp_reply_code(221));
                break; // bye bye!
            }
            else {
                rumble_comm_send(&session, rumble_smtp_reply_code(500));
            }
        }
        close(session.client->socket);
    }
    pthread_exit(0);
}
