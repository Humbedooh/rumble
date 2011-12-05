/* File: public.c Author: Humbedooh Created on January 7, 2011, 11:27 PM */
#include "rumble.h"
FILE            *sysLog = 0;
masterHandle    *public_master_handle = 0;
dvector			*debugLog = 0;

/*
 =======================================================================================================================
    This file contains public functions for rumble (usable by both server and modules
 =======================================================================================================================
 */
void rumble_args_free(rumble_args *d) {

    /*~~~~~~~~~~*/
    uint32_t    p;
    /*~~~~~~~~~~*/

    for (p = 0; p < d->argc; p++) {
        free(d->argv[p]);
    }

    free(d->argv);
    free(d);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_args *rumble_read_words(const char *d) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char        *s;
    ssize_t     a,
                b,
                c,
                x;
    rumble_args *ret = (rumble_args *) malloc(sizeof(rumble_args));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    ret->argv = (char **) calloc(1, 32 * sizeof(char *));
    a = 0;
    b = 0;
    c = 0;
    x = 0;
    ret->argc = 0;
    if (!d || !strlen(d)) return (ret);
    for (s = (char *) d; *s; s++) {
        b++;
        if (*s == '"') c++;
        if (c % 2 == 0 && *s == ' ') {
            x = (*(d + a) == '"') ? 1 : 0;
            if (b - a - x - 1 > 0) {
                ret->argv[ret->argc] = (char *) calloc(1, b - a - x + 1);
                strncpy(ret->argv[ret->argc++], d + a + x, b - a - x - x - 1);
            }

            a = b;
        }
    }

    if (b > a) {
        x = (*(d + a) == '"') ? 1 : 0;
        if (b - a - x - 1 > 0) {
            ret->argv[ret->argc] = (char *) calloc(1, b - a - x + 1);
            strncpy(ret->argv[ret->argc++], d + a + x, b - a - x - 1);
        }
    }

    return (ret);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
address *rumble_parse_mail_address(const char *addr) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    address *usr = (address *) malloc(sizeof(address));
    char    *tmp;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!usr) merror();
    if (!addr) return (0);
    usr->domain = (char *) calloc(1, 130);
    usr->user = (char *) calloc(1, 130);
    usr->raw = (char *) calloc(1, 256);
    usr->flags = dvector_init();
    usr->tag = (char *) calloc(1, 130);
    usr->_flags = (char *) calloc(1, 130);
    tmp = (char *) calloc(1, 260);
    if (!tmp) merror();
    if (strchr(addr, '<')) {
        addr = strchr(addr, '<');

        /* First, try to scan for "<user@domain> FLAGS" */
        if (sscanf(addr, "<%256[^@ ]@%128[^>]>%128[A-Z= %-]", tmp, usr->domain, usr->_flags) < 2) {

            /* Then, try scanning for "<> FLAGS" (bounce message) */
            sscanf(addr, "<%128[^>]>%128[A-Z= %-]", tmp, usr->_flags);
        }

        /* Parse any flags we find */
        if (strlen(usr->_flags)) rumble_scan_flags(usr->flags, usr->_flags);

        /* Separate username and VERP/BATV tag (if any) */
        if (sscanf(tmp, "prvs=%16[^+=]=%128c", usr->tag, usr->user) < 2) {

            /* Did we have a BATV tag? */
            if (sscanf(tmp, "%128[^=]=%128c", usr->tag, usr->user) < 2) {

                /* Or maybe a VERP? */
                strncpy(usr->user, tmp, 128);   /* Nope, just a username. */
                memset(usr->tag, 0, 128);
            }
        }
	// Try plain old "mail from: user@domain" ?
    } else if (strlen(addr)) {
		if (strstr(addr, ": ")) addr = strstr(addr, ": ") + 2;
		else if (strchr(addr, ':')) addr = strchr(addr, ':') + 1;
		else addr = 0;
        if (addr) sscanf(addr, "%128[^@ ]@%128c", usr->user, usr->domain);
    }

    if (!strlen(usr->user) or!strlen(usr->domain)) {
        rumble_free_address(usr);
        usr = 0;
    } else sprintf(usr->raw, "<%s@%s> %s", usr->user, usr->domain, usr->_flags ? usr->_flags : "NOFLAGS");
    free(tmp);
    return (usr);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_string_lower(char *d) {

    /*~~~~~~*/
    size_t  a,
            b;
    /*~~~~~~*/

    b = strlen(d);
    for (a = 0; a < b; a++) {
        d[a] = (d[a] >= 'A' && d[a] <= 'Z') ? d[a] + 32 : d[a];
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_string_upper(char *d) {

    /*~~~~~~*/
    size_t  a,
            b;
    /*~~~~~~*/

    b = strlen(d);
    for (a = 0; a < b; a++) {
        d[a] = (d[a] >= 'a' && d[a] <= 'z') ? d[a] - 32 : d[a];
    }
}

/*
 =======================================================================================================================
    Scans a string for key=value pairs and stores them in a dvector dictionary.
 =======================================================================================================================
 */
void rumble_scan_flags(dvector *dict, const char *flags) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *pch = strtok((char *) flags, " ");
    char    *key,
            *val;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    key = (char *) calloc(1, 100);
    val = (char *) calloc(1, 100);
    while (pch != NULL) {
        if (strlen(pch) >= 3) {
            memset(key, 0, 99);
            memset(val, 0, 99);
            if (sscanf(pch, "%99[^=]=%99c", key, val) >= 1) {
                rumble_string_upper(key);
                rsdict(dict, key, val);
            }
        }

        pch = strtok(NULL, " ");
    }
}

/*
 =======================================================================================================================
    Scans a string for words and stores them in a dvector dictionary as [word] => 1
 =======================================================================================================================
 */
void rumble_scan_words(dvector *dict, const char *wordlist) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *pch = strtok((char *) wordlist, " ");
    char    *key = (char *) calloc(1, 200);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!key) merror();
    while (pch != NULL) {
        strncpy(key, pch, 199);
        if (strlen(key) >= 1) {
            rumble_string_lower(key);
            rsdict(dict, key, "1");
        }

        pch = strtok(NULL, " ");
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
const char *rumble_get_dictionary_value(dvector *dict, const char *flag) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    dvector_element     *curr;
    rumbleKeyValuePair  *el;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    for (curr = dict->first; curr != NULL; curr = curr->next) {
        el = (rumbleKeyValuePair *) curr->object;
        if (!strcmp(flag, el->key)) return (el->value);
    }

    return ("0");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_has_dictionary_value(dvector *dict, const char *flag) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    dvector_element     *curr;
    rumbleKeyValuePair  *el;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    for (curr = dict->first; curr != NULL; curr = curr->next) {
        el = curr->object;
        if (!strcmp(flag, el->key) && strlen(el->value)) return (1);
    }

    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_add_dictionary_value(dvector *dict, const char *key, const char *value) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    char                *nkey,
                        *nval;
    rumbleKeyValuePair  *el;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    nkey = (char *) calloc(1, strlen(key) + 1);
    nval = (char *) calloc(1, strlen(value) + 1);
    if (!nkey || !nval) merror();
    strcpy(nval, value);
    strcpy(nkey, key);
    el = (rumbleKeyValuePair *) malloc(sizeof(rumbleKeyValuePair));
    if (!el) merror();
    el->key = (const char *) nkey;
    el->value = (const char *) nval;
    dvector_add(dict, el);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_flush_dictionary(dvector *dict) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    rumbleKeyValuePair  *el;
    d_iterator          iter;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    if (!dict) {
        return;
    }

    foreach((rumbleKeyValuePair *), el, dict, iter) {
        if (el->key) free((char *) el->key);
        if (el->value) free((char *) el->value);
        free(el);
    }

    dvector_flush(dict);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_free_address(address *a) {
    if (!a) return;
    if (a->domain) free(a->domain);
    if (a->raw) free(a->raw);
    if (a->user) free(a->user);
    if (a->_flags) free(a->_flags);
    if (a->tag) free(a->tag);
    rumble_flush_dictionary(a->flags);
    a->domain = 0;
    a->user = 0;
    a->raw = 0;
    a->_flags = 0;
    a->tag = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_mtime(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    time_t  rawtime;
    char    *txt = (char *) malloc(48);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!txt) merror();
    time(&rawtime);
    strftime(txt, 48, "%a, %d %b %Y %X +0000 (UTC)", gmtime(&rawtime));
    return (txt);
}

#if defined __x86_64__
#   define ptr2int(a)  (uint32_t) (((uintptr_t) a << 32))
#else
#   define ptr2int(a)  (uint32_t) a
#endif

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_create_filename(void) {

    /*~~~~~~~~~~~~~~~~~~*/
    char            *name;
    unsigned char   *p;
    uint32_t        y[4],
                    x;
    /*~~~~~~~~~~~~~~~~~~*/

    name = (char *) malloc(17);
    y[0] = time(NULL) - rand();
    y[1] = rand() * rand();
    y[3] = ptr2int(rumble_create_filename) * rand();
    y[2] = ptr2int(&y) - rand();
    p = (unsigned char *) y;
    for (x = 0; x < 16; x++) {
        name[x] = (p[x] % 26) + 'a';
    }

    name[16] = 0;
    return (name);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
size_t rumble_file_exists(const char *filename)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifdef RUMBLE_MSC
    FILE    *fd = fopen(filename, "r");
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (fd) {
        fclose(fd);
        return (1);
    }

#else
    if (access(filename, 0) == 0) return (1);
#endif
    else return (0);
    return (1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void statusLog(const char *msg, ...) {

    /*~~~~~~~~~~~~~~~~~~*/
    time_t      rawtime;
    struct tm   *timeinfo;
    char        txt[130];
    va_list     vl;
    int         rc = 0;
    /*~~~~~~~~~~~~~~~~~~*/

    if (sysLog) {
        va_start(vl, msg);
        time(&rawtime);
        timeinfo = gmtime(&rawtime);
        strftime(txt, 128, "%Y/%m/%d %X", timeinfo);
        fprintf(sysLog, "%s: \t", txt);
        vfprintf(sysLog, msg, vl);
        rc = fprintf(sysLog, "\r\n");
        fflush(sysLog);
		va_end(vl);
		va_start(vl, msg);
		rumble_vdebug("core", msg, vl);
    }
}


void rumble_vdebug(const char* svc, const char *msg, va_list args) {

    /*~~~~~~~~~~~~~~~~~~*/
    time_t      rawtime;
    struct tm   *timeinfo;
	char		dummy[512], txt[130];
	char		*dstring;
	dvector_element* obj;
    int         rc = 0;
    /*~~~~~~~~~~~~~~~~~~*/

    if (debugLog) {
		dstring = (char*) debugLog->last->object;
		obj = debugLog->last;
		obj->next = debugLog->first;
		obj->prev->next = 0;
		debugLog->last = obj->prev;
		debugLog->first->prev = obj;
		debugLog->first = obj;
		obj->prev = 0;
        time(&rawtime);
        timeinfo = gmtime(&rawtime);
        strftime(txt, 128, "%Y/%m/%d %X", timeinfo);
        sprintf(dummy, "%s [%s]: \t %s\r\n", txt, (svc ? svc : "core"), msg);
        vsprintf(dstring, dummy, args);
		vprintf(dummy, args);
    }
}

void rumble_debug(const char* svc, const char *msg, ...) {

    /*~~~~~~~~~~~~~~~~~~*/
    va_list     vl;
    /*~~~~~~~~~~~~~~~~~~*/

    if (debugLog) {
        va_start(vl, msg);
        rumble_vdebug(svc, msg, vl);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
lua_State *rumble_acquire_state(void) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
    int             found = 0;
    masterHandle    *master = public_master_handle;
    lua_State       *L;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    pthread_mutex_lock(&master->lua.mutex);
    for (x = 0; x < RUMBLE_LSTATES; x++) {
        if (!master->lua.states[x].working) {
            master->lua.states[x].working = 1;
            L = (lua_State *) master->lua.states[x].state;
            found = 1;
            break;
        }
    }

    pthread_mutex_unlock(&master->lua.mutex);
    if (found) return (L);
    else {
        sleep(1);
        return (rumble_acquire_state());
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_release_state(lua_State *X) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
    masterHandle    *master = public_master_handle;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    pthread_mutex_lock(&master->lua.mutex);
    for (x = 0; x < RUMBLE_LSTATES; x++) {
        if (master->lua.states[x].state == X) {
            master->lua.states[x].working = 0;
            break;
        }
    }

    pthread_mutex_unlock(&master->lua.mutex);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *strclone(const void *o) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    size_t  l = strlen((const char *) o);
    char    *ret = calloc(1, l + 1);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    strncpy(ret, (const char *) o, l);
    return (ret);
}
