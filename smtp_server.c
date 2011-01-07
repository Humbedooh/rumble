#include "rumble.h"
#include "servers.h"

void* rumble_smtp_init(void* m) {
    masterHandle* master = (masterHandle*) m;
    printf(".");
    while (1) {
        // Initialize a session handle and wait for incoming connections.
        sessionHandle session;
        session.client = (clientHandle*) comm_accept(master->smtp.socket);
        session.flags = 0;
        session.recipients = cvector_init();
        
        // Check for hooks on accept()
        ssize_t rc = EXIT_SUCCESS;
        rc = rumble_server_schedule_hooks(master, &session, RUMBLE_HOOK_ACCEPT + RUMBLE_HOOK_SMTP );
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
            if (!strcmp(cmd, "QUIT")) {
                rumble_comm_send(&session, rumble_smtp_reply_code(221));
                break; // bye bye!
            }
            else if (!strcmp(cmd, "MAIL")) {
                char* raw = calloc(1,1000);
                char* user = calloc(1,64);
                char* domain = calloc(1,128); // RFC says 64, but that was before i18n
                sscanf(arg, "%*4c:%1000c", raw);
                // Try to fetch standard syntax: MAIL FROM: [whatever] <user@domain.tld>
                sscanf(raw, "%*256[^<]<%64[^>@]@%128[^@>]", user, domain);
                // Set the current values
                session.sender.raw = raw;
                session.sender.user = user;
                session.sender.domain = domain;
                
            }
            else {
                rumble_comm_send(&session, rumble_smtp_reply_code(500)); // Bad mojo!
            }
        }
        // Cleanup
        close(session.client->socket);
    }
    pthread_exit(0);
}
