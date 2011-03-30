/*$I0 */
#include "database.h"
#ifdef MYSQL_CLIENT /* Only compile if mysql support is enabled */
extern masterHandle *rumble_database_master_handle;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *rumble_database_get_handle_mysql(void *pool) {
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_load_mysql(masterHandle *master, FILE *runlog) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    MYSQL       *mysql = mysql_init(0);
    my_bool     yes = 1;
    const char  *host,
                *user,
                *pass,
                *db;
    int         port;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mysql_options((MYSQL *) mysql, MYSQL_OPT_RECONNECT, &yes);
    host = rrdict(master->_core.conf, "mysqlhost");
    user = rrdict(master->_core.conf, "mysqluser");
    pass = rrdict(master->_core.conf, "mysqlpass");
    db = rrdict(master->_core.conf, "mysqldb");
    port = atoi(rrdict(master->_core.conf, "mysqlport"));
    printf("%-38s", "MySQL: Connecting....");
    if (!mysql_real_connect(mysql, host, user, pass, db, port, 0, 0)) {
        fprintf(stderr, "Failed to connect to database: Error: %s\n", mysql_error(mysql));
        statusLog("Failed to connect to database: Error: %s\n", mysql_error(mysql));
        exit(0);
    }

    master->_core.db = mysql;
    printf("[OK]\r\n");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_prepare_mysql(dbObject *dbo, const char *statement, va_list vl) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char                    *sql,
                            b;
    const char              *p,
                            *op;
    char                    injects[32];
    void                    *O;
    int                     rc;
    size_t                  len = 0,
                            strl = 0;
    int                     at = 0,
                            params;
    unsigned long           *str_len;
    unsigned char           object[1024];
    int                     used = 0;
    int                     ss = 0;
    MYSQL_BIND              *bindings;
    unsigned int            d_uint;
    signed int              d_sint;
    signed long long int    d_lint;
    double                  d_double;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("Preparing statement: %s\n", statement);
    memset(object, 0, 1024);
    memset(injects, 0, 32);
    sql = (char *) calloc(1, 2048);
    if (!sql) merror();
    op = statement;
    for (p = strchr(statement, '%'); p != NULL; p = strchr(op, '%')) {
        strl = strlen(op) - strlen(p);
        strncpy((char *) (sql + len), op, strl);
        len += strl;
        if (sscanf((const char *) p, "%%%c", &b)) {
            if (b == '%') {
                strncpy((char *) (sql + len), "%", 1);
                len += 1;
            } else {
                strncpy((char *) (sql + len), "?", 1);
                len += 1;
                injects[at++] = b;
            }

            op = (char *) p + 2;
        }
    }

    strl = strlen(op);
    strncpy((char *) (sql + len), op, strl);
    dbo->state = mysql_stmt_init((MYSQL *) rumble_database_master_handle->_core.db);
    rc = mysql_stmt_prepare((MYSQL_STMT *) dbo->state, sql, strl);
    free(sql);
    if (rc) {
        dbo->state = 0;
        return;
    }

    params = mysql_stmt_param_count((MYSQL_STMT *) dbo->state);
    bindings = calloc(sizeof(MYSQL_BIND), params ? params + 1 : 1);
    for (at = 0; injects[at] != 0; at++) {
        bindings[at].is_null = 0;
        bindings[at].length = 0;
        bindings[at].is_unsigned = 1;
        switch (injects[at])
        {
        case 's':
            str_len = malloc(sizeof(unsigned long));
            bindings[at].buffer_type = MYSQL_TYPE_STRING;
            bindings[at].buffer = (void *) va_arg(vl, const char *);
            *str_len = strlen((const char *) bindings[at].buffer);
            bindings[at].buffer_length = *str_len;
            break;

        case 'u':
            O = &(object[used]);
            d_uint = va_arg(vl, unsigned int);
            ss = sizeof(unsigned int);
            memcpy(O, &d_uint, ss);
            bindings[at].buffer_type = MYSQL_TYPE_LONG;
            bindings[at].buffer = (void *) O;
            used += ss;
            break;

        case 'i':
            bindings[at].is_unsigned = 0;
            O = &(object[used]);
            d_sint = va_arg(vl, signed int);
            ss = sizeof(signed int);
            memcpy(O, &d_sint, ss);
            bindings[at].buffer_type = MYSQL_TYPE_LONG;
            bindings[at].buffer = (void *) O;
            used += ss;
            break;

        case 'l':
            bindings[at].is_unsigned = 0;
            bindings[at].buffer_type = MYSQL_TYPE_LONGLONG;
            ss = sizeof(signed long long int);
            O = &(object[used]);
            d_lint = va_arg(vl, signed long long int);
            memcpy(O, &d_lint, ss);
            bindings[at].buffer_type = MYSQL_TYPE_LONG;
            bindings[at].buffer = (void *) O;
            used += ss;
            break;

        case 'f':
            bindings[at].is_unsigned = 0;
            bindings[at].buffer_type = MYSQL_TYPE_DOUBLE;
            O = &(object[used]);
            d_double = va_arg(vl, double);
            ss = sizeof(double);
            memcpy(O, &d_double, ss);
            bindings[at].buffer = (void *) O;
            used += ss;
            break;

        default:
            break;
        }

        if (mysql_stmt_bind_param((MYSQL_STMT *) dbo->state, bindings)) {
            dbo->state = 0;
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dbResult *rumble_database_fetch_row_mysql(void *state) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int         count,
                i;
    dbObject    *dbo = (dbObject *) state;
    MYSQL_RES   *meta;
    MYSQL_BIND  *bindings;
    dbResult    *res = malloc(sizeof(dbResult));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    meta = mysql_stmt_result_metadata((MYSQL_STMT *) dbo->state);
    count = mysql_stmt_field_count((MYSQL_STMT *) dbo->state);
    bindings = calloc(sizeof(MYSQL_BIND), count);
    res->column = malloc(count * sizeof(dbItem));
    res->items = count;
    for (i = 0; i < count; i++) {
        res->column[i].data = calloc(1, meta->fields[i].length);
        bindings[i].buffer = res->column[i].data;
        res->column[i].type = (meta->fields[i].type == MYSQL_TYPE_STRING) ? 1 : 2;
        bindings[i].buffer_type = meta->fields[i].type;
    }

    mysql_stmt_bind_result((MYSQL_STMT *) dbo->state, bindings);
    mysql_stmt_store_result((MYSQL_STMT *) dbo->state);
    mysql_free_result(meta);
    free(bindings);
    return (res);
}
#endif
