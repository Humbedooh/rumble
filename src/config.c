/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include <stdarg.h>
typedef struct
{
    const char      *key;
    unsigned int    val;
} _cft;
static _cft rumble_conf_tags[] =
{
    { "windows", R_WINDOWS },
    { "unix", R_POSIX && !R_CYGWIN },
    { "cygwin", R_CYGWIN },
    { "x64", R_ARCH == 64 },
    { "x86", R_ARCH == 32 },
    { 0, 0 }
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_config_load(masterHandle *master, dvector *args) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char                *paths[3] = { "config", "/var/rumble/config", "C:/cygwin/home/Administrator/rumble/config" };
    char                *cfgfile;
    const char          *cfgpath;
    FILE                *config;
    rumbleKeyValuePair  *el;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    master->_core.conf = dvector_init();
    cfgfile = (char *) calloc(1, 1024);
    cfgpath = rumble_get_dictionary_value(args, "--CONFIG-DIR");
    if (!cfgfile) merror();
    if (strlen(cfgpath) && strcmp(cfgpath, "0")) {
        el = (rumbleKeyValuePair *) malloc(sizeof(rumbleKeyValuePair));
        if (!el) merror();
        el->key = "config-dir";
        el->value = rumble_get_dictionary_value(args, "--CONFIG-DIR");
        dvector_add(master->_core.conf, el);
        sprintf(cfgfile, "%s/rumble.conf", el->value);
        master->cfgdir = el->value;
    } else {

        /*~~~~~~*/
        int x = 0;
        /*~~~~~~*/

        for (x = 0; x < 3; x++) {
            sprintf(cfgfile, "%s/rumble.conf", paths[x]);
            config = fopen(cfgfile, "r");
            if (config) {
                fclose(config);
                el = (rumbleKeyValuePair *) malloc(sizeof(rumbleKeyValuePair));
                if (!el) merror();
                el->key = "config-dir";
                el->value = paths[x];
                dvector_add(master->_core.conf, el);
                master->cfgdir = el->value;
                break;
            }
        }
    }

#define CYGWIN  __CYGWIN__
    config = fopen(cfgfile, "r");
    if (config) {

        /*~~~~~~~~~~~~~~~~~~~~~~~*/
        int             p = 0;
        unsigned int    ignore = 0,
                        n = 0;
        char            *key,
                        *value,
                        *buffer,
                        line[512];
        /*~~~~~~~~~~~~~~~~~~~~~~~*/

        buffer = (char *) malloc(512);
        key = (char *) calloc(1, 512);
        value = (char *) calloc(1, 512);
        if (!buffer || !key || !value) merror();
        while (!feof(config)) {
            memset(buffer, 0, 512);
            memset(line, 0, 512);
            fgets(buffer, 511, config);
            if (sscanf(buffer, "%*[ \t]%511[^\r\n]", line) == 0) sscanf(buffer, "%511[^\r\n]", line);
            p++;
            if (!ferror(config)) {
                memset(key, 0, 512);
                memset(value, 0, 512);
                if (sscanf(line, "<%511[^ \t>]%*[ \t]%511[^\r\n>]", key, value) >= 1) {
                    rumble_string_lower(key);
                    rumble_string_lower(value);
                    if (!strcmp(key, "/if")) ignore >>= 1;
                    if (!strcmp(key, "if") && !ignore) {
                        ignore = (ignore << 1) + 1;
                        for (n = 0; rumble_conf_tags[n].key; n++)
                            if (!strcmp(value, rumble_conf_tags[n].key) && rumble_conf_tags[n].val) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else-if")) {
                        ignore &= 0xFFFFFFFE;
                        ignore++;
                        for (n = 0; rumble_conf_tags[n].key; n++)
                            if (!strcmp(value, rumble_conf_tags[n].key) && rumble_conf_tags[n].val) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else") && !(ignore & 0xFFFFFFFE)) ignore = (ignore & 0xFFFFFFFE) | (!(ignore & 0x00000001));
                }

                if (sscanf(line, "%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    rsdict(master->_core.conf, key, value);
                } else if (sscanf(line, "%*[ \t]%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    rsdict(master->_core.conf, key, value);
                }
            } else {
                fprintf(stderr, "<config> Error: Could not read %s!\n", cfgfile);
                exit(EXIT_FAILURE);
            }
        }

        free(buffer);
        fclose(config);
    } else {
        fprintf(stderr, "<config> Error: Could not read %s!\n", cfgfile);
        exit(EXIT_FAILURE);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
const char *rumble_config_str(masterHandle *master, const char *key) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    rumbleKeyValuePair  *el;
    d_iterator          iter;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    foreach((rumbleKeyValuePair *), el, master->_core.conf, iter) {
        if (!strcmp(el->key, key)) {
            return (const char *) el->value;
        }
    }

    return (const char *) "";
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_config_int(masterHandle *master, const char *key) {

    /*~~~~~~~~~~~~~~~~~~~~~*/
    rumbleKeyValuePair  *el;
    d_iterator          iter;
    /*~~~~~~~~~~~~~~~~~~~~~*/

    foreach((rumbleKeyValuePair *), el, master->_core.conf, iter) {
        if (!strcmp(el->key, key)) {
            return (atoi(el->value));
        }
    }

    return (0);
}
