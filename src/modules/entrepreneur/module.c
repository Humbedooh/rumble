/* 
 * File:   module.c
 * Author: Humbedooh
 * 
 * An administration interface for rumble
 * Created on January 3, 2011, 8:08 PM
 */
#include "../../rumble.h"
#include "../../comm.h"

void rumble_scan_formdata(cvector* dict, const char* flags){
    char* pch = strtok((char*) flags,"&");
    while ( pch != NULL ) {
        if ( strlen(pch) >= 3 ) {
            char* key = calloc(1, 100);
            char* value = calloc(1, 256);
            sscanf(pch, "%100[^=]=%250c", key, value);
            rumble_string_lower(key);
            rsdict(dict, key, value);
        }
        pch = strtok(NULL, "&");
    }
}


void* accept_connection(void* m); // Prototype
rumbleService* svc;

// Standard module initialization function
int __declspec(dllexport) rumble_module_init(void* m, rumble_module_info* modinfo) {
	int n;
    svc = (rumbleService*) calloc(1,sizeof(rumbleService));
    modinfo->title = "Entrepreneur module";
    modinfo->description = "Module for administering stuff";
    modinfo->author = "Humbedooh";
    svc->socket = comm_init((masterHandle*) m, "80");
    if ( ! svc->socket) { printf("bleh, no socket :(\n"); exit(0); }
    svc->threads = cvector_init();
   
    for ( n = 0; n < 10; n++) {
        pthread_t* t = malloc(sizeof(pthread_t));
        cvector_add(svc->threads, t);
        pthread_create(t, NULL, accept_connection, m);
    }
   rumble_hook_function(m,0,0); // make a blank hook to catch rumble_version_check from librumble
   return EXIT_SUCCESS; // Tell the thread to continue.
}

void* accept_connection(void* m) {
    masterHandle* master = (masterHandle*) m;
	cvector *args, *form;
	char* postBuffer, *URL, *rest, *now;
	const char* URI;
	ssize_t rc;
	rumble_module_info* mod;
    // Initialize a session handle and wait for incoming connections.
    sessionHandle session;
    sessionHandle* s = &session;
    clientHandle client;
    session.client = &client;
    session._master = master;
	
    args = cvector_init();
    form = cvector_init();
    
    while (1) {
        postBuffer = 0;
        comm_accept(svc->socket, &client);
        while (1) {
            char* line = rumble_comm_read(s);
            char* cmd = calloc(1,32);
            char* arg = calloc(1,128);
            sscanf(line, "%30[^ \r\n:]%*[: ]%120[^\r\n]", cmd, arg);
            if ( strlen(cmd) ) {
                rumble_string_lower(cmd);
                if (strlen(arg)) rsdict(args, cmd, arg);
                if ( !strcmp(cmd, "content-length")) {
                    uint32_t len = atoi(arg);
                    if ( len > (1024*1024)) len = 1024*1024;
                    postBuffer = calloc(1, len+1);
                    rc = recv(client.socket, postBuffer, len, 0);
                    if ( rc < 1) { free(postBuffer); postBuffer = 0;}
                    break;
                }
                free(cmd);
                free(arg);
            }
            else break;
        }
        URI = strlen(rrdict(args, "get")) ? rrdict(args, "get") : rrdict(args, "post");
        URL = calloc(1, strlen(URI));
        rest = calloc(1, strlen(URI));
        sscanf(URI, "/%200[^? ]?%200[^ ]", URL, rest);
        if ( strlen(rest)) rumble_scan_formdata(form, rest);
        if ( postBuffer) { rumble_scan_formdata(form, postBuffer); free(postBuffer); }
        rcsend(s, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        rcsend(s, "<h1>Rumble admin page</h1>\n");
        rcsend(s, "<h2>Enabled modules:</h2>");
        for (mod = cvector_first(master->readOnly.modules); mod != NULL; mod = cvector_next(master->readOnly.modules)) {
            rcprintf(s, "<b>%s</b>: %s (<small><font color='red'>%s</font></small>)<br/>\n", mod->title, mod->description, mod->file);
        }
        now = rumble_mtime();
        rcprintf(s,"<br/><br/><hr/><small>Rumble (mod_entrepreneur) v/%x on %s - %s</small>", RUMBLE_VERSION, rumble_config_str(master, "servername"), now);
        free(now);
        close(session.client->socket);
        rumble_flush_dictionary(args);
        rumble_flush_dictionary(form);
        
    }
}
// Done!
