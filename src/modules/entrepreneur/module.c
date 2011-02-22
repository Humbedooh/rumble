/*
 * File: module.c Author: Humbedooh An administration interface for rumble Created
 * on January 3, 2011, 8:08 P
 */
#include "../../rumble.h"
#include "../../comm.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_scan_formdata(dvector *dict, const char *flags) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *pch = strtok((char *) flags, "&");
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    while (pch != NULL) {
        if (strlen(pch) >= 3) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char    *key = (char *) calloc(1, 100);
            char    *value = (char *) calloc(1, 256);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            sscanf(pch, "%100[^=]=%250c", key, value);
            rumble_string_lower(key);
            rsdict(dict, key, value);
        }

        pch = strtok(NULL, "&");
    }
}

void            *accept_connection(void *m);    /* Prototype */
rumbleService   *svc;
char            *html_template;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *entr_format_page(masterHandle *m, dvector *dict) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *p,
            *op,
            *key,
            *x;
    ssize_t len = 0,
            strl = 0;
    char    *ret = (char *) calloc(1, 256 * 1024);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    key = (char *) malloc(256);
    if (!ret || !key) merror();
    op = html_template;
    for (p = strstr(html_template, "[rumble::"); p != NULL; p = strstr(op, "[rumble::")) {
        strl = strlen(op) - strlen(p);
        strncpy((char *) (ret + len), op, strl);
        len += strl;
        memset(key, 0, 256);
        sscanf((const char *) p, "[rumble::%100[a-z_]]", key);
        if (!strcmp(key, "date")) {
            x = rumble_mtime();
            strncpy((char *) (ret + len), x, strlen(x));
            len += strlen(x);
            free(x);
        } else if (!strcmp(key, "title")) {
            x = "Rumble mail server";
            strncpy((char *) (ret + len), x, strlen(x));
            len += strlen(x);
        } else if (!strcmp(key, "host")) {
            x = (char *) rrdict(m->_core.conf, "servername");
            strncpy((char *) (ret + len), x, strlen(x));
            len += strlen(x);
        } else if (!strcmp(key, "version")) {
            x = (char *) calloc(1, 10);
            sprintf(x, "%X", RUMBLE_VERSION);
            strncpy((char *) (ret + len), x, strlen(x));
            len += strlen(x);
            free(x);
        } else {
            if (rhdict(dict, key)) {
                x = (char *) rrdict(dict, key);
                strncpy((char *) (ret + len), x, strlen(x));
                len += strlen(x);
            }
        }

        op = (char *) p + strlen(key) + 10;
    }

    return (ret);
}

/*
 =======================================================================================================================
    Standard module initialization function
 =======================================================================================================================
 */
rumblemodule rumble_module_init(void *m, rumble_module_info *modinfo) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             n;
    long            fs;
    masterHandle    *master = (masterHandle *) m;
    FILE            *fp;
    char            *path = (char *) calloc(1, 256);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!path) merror();
    sprintf(path, "%s/entrepreneur/template.html", master->cfgdir);
    fp = fopen(path, "r");
    if (!fp) {
        perror("Couldn't open [cfgdir]/entrepreneur/template.html");
        return (EXIT_SUCCESS);
    }

    fseek(fp, 0, SEEK_END);
    fs = ftell(fp);
    html_template = (char *) malloc(fs);
    if (!html_template) merror();
    rewind(fp);
    fread(html_template, 1, fs, fp);
    fclose(fp);
    svc = (rumbleService *) calloc(1, sizeof(rumbleService));
    if (!svc) merror();
    modinfo->title = "Entrepreneur module";
    modinfo->description = "Module for administering stuff";
    modinfo->author = "Humbedooh";
    svc->socket = comm_init((masterHandle *) m, "80");
    if (!svc->socket) {
        printf("bleh, no socket :(\n");
        return (EXIT_FAILURE);
    }

    svc->threads = dvector_init();
    for (n = 0; n < 10; n++) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        pthread_t   *t = (pthread_t *) malloc(sizeof(pthread_t));
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        dvector_add(svc->threads, t);
        pthread_create(t, NULL, accept_connection, m);
    }

    rumble_hook_function(m, 0, 0);  /* make a blank hook to catch rumble_version_check from librumble */
    return (EXIT_SUCCESS);          /* Tell the thread to continue. */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
typedef struct {
    char*       URL;
    dvector* form;
    dvector* headers;
    const char* method;
    const char* remote_addr;
    sessionHandle* session;
    
} HTTPRequest;

void free_session(HTTPRequest* http) {
    rumble_flush_dictionary(http->headers);
    rumble_flush_dictionary(http->form);
    free(http->URL);
    free(http);
}
HTTPRequest* parse_session(sessionHandle* s) {
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *cmd = (char *) calloc(1, 32);
    char    *arg = (char *) calloc(1, 128);
    char    *line;
    const char *URI;
    char* rest;
    char*postBuffer = 0;
    int rc;
    HTTPRequest* http;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    http = (HTTPRequest*) malloc(sizeof(HTTPRequest));
    http->form = dvector_init();
    http->headers = dvector_init();
    http->remote_addr = s->client->addr;
    http->session = s;
    
    while (1) {
        memset(cmd, 0, 32);
        memset(arg, 0, 128);
            line = rumble_comm_read(s);
            sscanf(line, "%30[^ \r\n:]%*[: ]%120[^\r\n]", cmd, arg);
            if (strlen(cmd)) {
                rumble_string_lower(cmd);
                if (strlen(arg)) rsdict(http->headers, cmd, arg);
                if (!strcmp(cmd, "content-length")) {

                    /*~~~~~~~~~~~~~~~~~~~~~~~~*/
                    uint32_t    len = atoi(arg);
                    /*~~~~~~~~~~~~~~~~~~~~~~~~*/

                    if (len > (1024 * 1024)) len = 1024 * 1024;
                    postBuffer = (char *) calloc(1, len + 1);
                    if (!postBuffer) merror();
                    rc = recv(s->client->socket, postBuffer, len, 0);
                    if (rc < 1) {
                        free(postBuffer);
                        postBuffer = 0;
                    }
                    break;
                }

                
            } else break;
        }
free(cmd);
                free(arg);
        URI = rhdict(http->headers, "get") ? rrdict(http->headers, "get") : rrdict(http->headers, "post");
        http->method = rhdict(http->headers, "get") ? "get" : "post";
        http->URL = (char *) calloc(1, strlen(URI));
        rest = (char *) calloc(1, strlen(URI));
        if (!http->URL || !rest) merror();
        sscanf(URI, "/%200[^? ]?%[^ ]", http->URL, rest);
        if (strlen(rest)) rumble_scan_formdata(http->form, rest);
        if (postBuffer) {
            rumble_scan_formdata(http->form, postBuffer);
            free(postBuffer);
        }
        free(rest);
        return http;
}

void *accept_connection(void *m) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    masterHandle        *master = (masterHandle *) m;

    dvector                    *dict;
    d_iterator          iter;
    char                
                        
                        *now,
                        *output;
    char                buffa[1024 * 32],
                        buffb[1024 * 32];
    int                 myPos;


    rumble_module_info  *mod;
    /* Initialize a session handle and wait for incoming connections. */
    sessionHandle       session;
    sessionHandle       *s = &session;
    clientHandle        client;
    HTTPRequest*        http;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    session.client = &client;
    session._master = master;
    client.send = 0;
    client.recv = 0;
    dict = dvector_init();
    while (1) {
        
        comm_accept(svc->socket, &client);
        
        http = parse_session(s);

        myPos = 0;
        dforeach((rumble_module_info *), mod, master->_core.modules, iter) {
            sprintf(buffa, "<b>%s</b> <small>(<font color='red'>%s</font>)</small>: <br/> %s<hr/><br/>\n", mod->title, mod->file,
                    mod->description);
            strncpy(&buffb[myPos], buffa, strlen(buffa));
            myPos += strlen(buffa);
        }

        rsdict(dict, "modules", buffb);
        now = rumble_mtime();
        free(now);
        output = entr_format_page(master, dict);
        rcsend(s, output);
        free(output);
        close(session.client->socket);
        free_session(http);
    }
    return 0;
}

/* Done! */
