/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include <stdarg.h>
#include <fcntl.h>
dvector *realargs = 0;
typedef struct
{
    const char  *key;
    const char  *val;
} _cft;
static _cft rumble_conf_tags[] =
{
    { "windows", R_WINDOWS ? "1" : "" },
    { "unix", R_POSIX && !R_CYGWIN ? "1" : "" },
    { "linux", R_LINUX ? "1" : "" },
    { "cygwin", R_CYGWIN ? "1" : "" },
    { "x64", R_ARCH == 64 ? "1" : "" },
    { "x86", R_ARCH == 32 ? "1" : "" },
    { "architecture", R_ARCH == 32 ? "32" : "64" },
    { 0, 0 }
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static int rumble_compare_value(dvector *config, const char *key, const char *oper, const char *value) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char  *cval = rrdict(config, key);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!cval) return (0);
    if (!strcmp(oper, "=")) return (!strcmp(value, cval));
    if (!strcmp(oper, ">")) return (atoi(value) < atoi(cval));
    if (!strcmp(oper, "<")) return (atoi(value) > atoi(cval));
    if (!strcmp(oper, ">=")) return (atoi(value) <= atoi(cval));
    if (!strcmp(oper, "<=")) return (atoi(value) >= atoi(cval));
    if (!strcmp(oper, "!=")) return (atoi(value) != atoi(cval));
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_config_load(masterHandle *master, dvector *args) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char                *paths[4] = { "config", "/var/rumble/config", "/etc/rumble/config", "/rumble/config" };
    char                *cfgfile;
    const char          *cfgpath;
    FILE                *config;
    rumbleKeyValuePair  *el;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    master->_core.conf = dvector_init();
    cfgfile = (char *) calloc(1, 1024);
    if (!args) args = realargs;
    realargs = args;
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

        for (x = 0; x < 4; x++) {
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

        /*~~~~~~~~~~~~~~~~~~~~~~~~*/
        int             p = 0;
        unsigned int    ignore = 0,
                        n = 0;
        char            key[512],
                        value[512],
                        buffer[512],
                        line[512],
                        ckey[129],
                        coper[3],
                        cval[129];
        /*~~~~~~~~~~~~~~~~~~~~~~~~*/

        for (n = 0; rumble_conf_tags[n].key; n++) rsdict(master->_core.conf, rumble_conf_tags[n].key, rumble_conf_tags[n].val);
        memset(ckey, 0, 129);
        memset(cval, 0, 129);
        memset(coper, 0, 3);
        while (!feof(config)) {
            memset(buffer, 0, 512);
            memset(line, 0, 512);
            if (!fgets(buffer, 511, config)) break;
            if (sscanf(buffer, "%*[ \t]%511[^\r\n]", line) == 0) sscanf(buffer, "%511[^\r\n]", line);
            p++;
            if (!ferror(config)) {
                memset(key, 0, 512);
                memset(value, 0, 512);
                if (sscanf(line, "<%511[^ \t>]%*[ \t]%511[^\r\n]>", key, value) >= 1) {
                    rumble_string_lower(key);
                    rumble_string_lower(value);
                    if (!strcmp(key, "/if")) ignore >>= 1;
                    if (!strcmp(key, "if")) {
                        ignore = (ignore << 1) + 1;
                        if (sscanf(value, "compare(%128[^) \t]%*[ \t]%128[^) \t]%*[ \t]%128[^) \t]%*[ \t])", ckey, coper, cval) == 3)
                            if (rumble_compare_value(master->_core.conf, ckey, coper, cval)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "defined(%128[^) \t])", ckey) == 1)
                            if (rhdict(master->_core.conf, ckey)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "exists(%128[^) \t])", ckey) == 1)
                            if (rumble_file_exists(ckey)) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else-if") && !(ignore & 0xFFFFFFFE)) {
                        ignore &= 0xFFFFFFFE;
                        ignore++;
                        if (sscanf(value, "compare(%128[^) \t]%*[ \t]%128[^) \t]%*[ \t]%128[^) \t]%*[ \t])", ckey, coper, cval) == 3)
                            if (rumble_compare_value(master->_core.conf, ckey, coper, cval)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "defined(%128[^) \t])", ckey) == 1)
                            if (rhdict(master->_core.conf, ckey)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "exists(%128[^) \t])", ckey) == 1)
                            if (rumble_file_exists(ckey)) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else") && !(ignore & 0xFFFFFFFE)) ignore = (ignore & 0xFFFFFFFE) | (!(ignore & 0x00000001));
                }

                if (sscanf(line, "%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    if (!strcmp(key, "comment")) {
                        printf("%s\r\n", value);
                        rumble_debug(NULL, "config", "%s", value);
                    } else rsdict(master->_core.conf, key, value);
                } else if (sscanf(line, "%*[ \t]%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    rsdict(master->_core.conf, key, value);
                }
            } else {
                rumble_debug(NULL, "config", "ERROR: Could not read %s!", cfgfile);
                free(cfgfile);
                exit(EXIT_FAILURE);
            }
        }

        fclose(config);
    } else {
        rumble_debug(NULL, "config", "ERROR: Could not open %s!", cfgfile);
        free(cfgfile);
        exit(EXIT_FAILURE);
    }

    free(cfgfile);
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

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dvector *rumble_readconfig(const char *filename) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char                *paths[3] = { "config", "/var/rumble/config", "/rumble/config" };
    char                cfgfile[1024];
    FILE                *config;
    rumbleKeyValuePair  *el;
    dvector             *configFile;
    int                 x = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    configFile = dvector_init();
    for (x = 0; x < 3; x++) {
        sprintf(cfgfile, "%s/%s", paths[x], filename);
        config = fopen(cfgfile, "r");
        if (config) {
            fclose(config);
            el = (rumbleKeyValuePair *) malloc(sizeof(rumbleKeyValuePair));
            if (!el) merror();
            el->key = "config-dir";
            el->value = paths[x];
            dvector_add(configFile, el);
            break;
        }
    }

#define CYGWIN  __CYGWIN__
    config = fopen(cfgfile, "r");
    if (config) {

        /*~~~~~~~~~~~~~~~~~~~~~~~~*/
        int             p = 0;
        unsigned int    ignore = 0,
                        n = 0;
        char            key[512],
                        value[512],
                        buffer[512],
                        line[512],
                        ckey[129],
                        coper[3],
                        cval[129];
        /*~~~~~~~~~~~~~~~~~~~~~~~~*/

        for (n = 0; rumble_conf_tags[n].key; n++) rsdict(configFile, rumble_conf_tags[n].key, rumble_conf_tags[n].val);
        memset(ckey, 0, 129);
        memset(cval, 0, 129);
        memset(coper, 0, 3);
        while (!feof(config)) {
            memset(buffer, 0, 512);
            memset(line, 0, 512);
            if (!fgets(buffer, 511, config)) break;
            if (sscanf(buffer, "%*[ \t]%511[^\r\n]", line) == 0) sscanf(buffer, "%511[^\r\n]", line);
            p++;
            if (!ferror(config)) {
                memset(key, 0, 512);
                memset(value, 0, 512);
                if (sscanf(line, "<%511[^ \t>]%*[ \t]%511[^\r\n]>", key, value) >= 1) {
                    rumble_string_lower(key);
                    rumble_string_lower(value);
                    if (!strcmp(key, "/if")) ignore >>= 1;
                    if (!strcmp(key, "if")) {
                        ignore = (ignore << 1) + 1;
                        if (sscanf(value, "compare(%128[^) \t]%*[ \t]%128[^) \t]%*[ \t]%128[^) \t]%*[ \t])", ckey, coper, cval) == 3)
                            if (rumble_compare_value(configFile, ckey, coper, cval)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "defined(%128[^) \t])", ckey) == 1)
                            if (rhdict(configFile, ckey)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "exists(%128[^) \t])", ckey) == 1)
                            if (rumble_file_exists(ckey)) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else-if") && !(ignore & 0xFFFFFFFE)) {
                        ignore &= 0xFFFFFFFE;
                        ignore++;
                        if (sscanf(value, "compare(%128[^) \t]%*[ \t]%128[^) \t]%*[ \t]%128[^) \t]%*[ \t])", ckey, coper, cval) == 3)
                            if (rumble_compare_value(configFile, ckey, coper, cval)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "defined(%128[^) \t])", ckey) == 1)
                            if (rhdict(configFile, ckey)) ignore &= 0xFFFFFFFE;
                        if (sscanf(value, "exists(%128[^) \t])", ckey) == 1)
                            if (rumble_file_exists(ckey)) ignore &= 0xFFFFFFFE;
                    }

                    if (!strcmp(key, "else") && !(ignore & 0xFFFFFFFE)) ignore = (ignore & 0xFFFFFFFE) | (!(ignore & 0x00000001));
                }

                if (sscanf(line, "%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    if (!strcmp(key, "comment")) {
                        printf("%s\r\n", value);
                        rumble_debug(NULL, "config", "%s", value);
                    } else rsdict(configFile, key, value);
                } else if (sscanf(line, "%*[ \t]%511[^# \t]%*[ \t]%511[^\r\n]", key, value) == 2 && !ignore) {
                    rumble_string_lower(key);
                    rsdict(configFile, key, value);
                }
            } else {
                rumble_debug(NULL, "config", "ERROR: Could not read %s!", cfgfile);
                exit(EXIT_FAILURE);
            }
        }

        fclose(config);
    } else {
        rumble_debug(NULL, "config", "ERROR: Could not open %s!", cfgfile);
        exit(EXIT_FAILURE);
    }

    return (configFile);
}
