/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/* define RADB_DEBUG 1 */
#include "radb.h"
#ifndef _SQLITE3_H_
#   define _SQLITE3_H_
#endif
using namespace std;
/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbObject *radb_init_object(radbMaster *dbm) {

    /*~~~~~~~~~~~~~*/
    radbObject  *dbo;
    /*~~~~~~~~~~~~~*/

    if (!dbm) {
        printf("[RADB] Received a null-pointer as radbm!\r\n");
        return (0);
    }

    dbo = (radbObject *) calloc(1, sizeof(radbObject));
    dbo->master = dbm;
    dbo->result = 0;
    dbo->inputBindings = 0;
    dbo->lastError = 0;
    memset(dbo->inputs, 0, 64);
    dbo->status = RADB_EMPTY;
#ifdef _SQLITE3_H_
    if (dbm->dbType == RADB_SQLITE3) dbo->db = dbm->handle;
#endif
#ifdef MYSQL_CLIENT
    if (dbm->dbType == RADB_MYSQL) dbo->db = radb_get_handle_mysql(&dbm->pool);
#endif
    return (dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void radb_cleanup(radbObject *dbo) {
    if (!dbo) return;
#ifdef RADB_DEBUG
    printf("Cleaning up\r\n");
#endif
#ifdef _SQLITE3_H_
    if (dbo->master->dbType == RADB_SQLITE3) {
        if (dbo->state) sqlite3_finalize((sqlite3_stmt *) dbo->state);
    }
#endif
#ifdef MYSQL_CLIENT
    if (dbo->master->dbType == RADB_MYSQL)
    {
#   ifdef RADB_DEBUG
        printf("Closing state\r\n");
#   endif
        if (dbo->state) mysql_stmt_close((MYSQL_STMT *) dbo->state);
#   ifdef RADB_DEBUG
        printf("Releasing handle\r\n");
#   endif
        radb_release_handle_mysql(&dbo->master->pool, dbo->db);
    }
#endif
#ifdef RADB_DEBUG
    printf("Calling radb_free_result\r\n");
#endif
    if (dbo->result) radb_free_result(dbo->result);
    if (dbo->inputBindings) free(dbo->inputBindings);
    free(dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void radb_close(radbMaster *dbm) {
    if (!dbm) return;
#ifdef _SQLITE3_H_
    if (dbm->dbType == RADB_SQLITE3) {
        sqlite3_close((sqlite3 *) dbm->handle);
        free(dbm);
        return;
    }
#endif
#ifdef MYSQL_CLIENT
    if (dbm->dbType == RADB_MYSQL) {

        /*~~~~~~~~~~~~~~*/
        unsigned int    i;
        /*~~~~~~~~~~~~~~*/

        for (i = 0; i < dbm->pool.count; i++) {
            mysql_close((MYSQL *) dbm->pool.children[i].handle);
        }

        free(dbm->pool.children);
        free(dbm);
        return;
    }
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
const char *radb_last_error(radbObject *dbo) {
    if (!dbo) return ("(null)");
    return (dbo->lastError ? dbo->lastError : "No error");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbResult *radb_step(radbObject *dbo) {
    if (!dbo) return (0);
    if (dbo->state == 0) {
        fprintf(stderr, "[RADB] Can't step: Statement wasn't prepared properly!\r\n");
        return (0);
    }

#ifdef _SQLITE3_H_
    if (dbo->master->dbType == RADB_SQLITE3) return (radb_fetch_row_sqlite(dbo));
#endif
#ifdef MYSQL_CLIENT
    if (dbo->master->dbType == RADB_MYSQL) return (radb_fetch_row_mysql(dbo));
#endif
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbObject *radb_prepare_vl(radbMaster *dbm, const char *statement, va_list vl) {

    /*~~~~~~~~~~~~~~~~~*/
    char        *sql,
                b;
    const char  *p,
                *op;
    size_t      len = 0,
                strl = 0;
    int         at = 0,
                rc = 0;
    radbObject  *dbo;
    /*~~~~~~~~~~~~~~~~~*/

    dbo = radb_init_object(dbm);
    if (!dbo) return (0);
    dbo->status = 0;
    sql = (char *) calloc(1, 2048);
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
                dbo->inputs[at++] = b;
            }

            op = (char *) p + 2;
        }
    }

    strl = strlen(op);
    strncpy((char *) (sql + len), op, strl);
    strl = strlen(sql);
    if (sql[strl - 1] != ';') sql[strl++] = ';';
    dbo->status = RADB_PARSED;
#ifdef MYSQL_CLIENT
    if (dbm->dbType == RADB_MYSQL) {
        dbo->state = mysql_stmt_init((MYSQL *) dbo->db);
        rc = mysql_stmt_prepare((MYSQL_STMT *) dbo->state, sql, (unsigned long) strl);
    }
#endif
#ifdef _SQLITE3_H_
    if (dbm->dbType == RADB_SQLITE3) {
        rc = sqlite3_prepare_v2((sqlite3 *) dbo->db, sql, -1, (sqlite3_stmt **) &dbo->state, NULL);
    }
#endif

    /*
     * printf("Prepared: %s\n", sql);
     */
    free(sql);
    if (rc) {
        radb_cleanup(dbo);
        return (0);
    }

    radb_inject_vl(dbo, vl);
    return (dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbObject *radb_prepare(radbMaster *dbm, const char *statement, ...) {

    /*~~~~~~~~~~~~~*/
    radbObject  *dbo;
    va_list     vl;
    /*~~~~~~~~~~~~~*/

#ifdef RADB_DEBUG
    printf("radb_prepare: %s\r\n", statement);
#endif
    va_start(vl, statement);
    dbo = radb_prepare_vl(dbm, statement, vl);
    va_end(vl);
    return (dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int radb_inject(radbObject *dbo, ...) {

    /*~~~~~~~*/
    int     rc;
    va_list vl;
    /*~~~~~~~*/

    if (!dbo) return (0);
    va_start(vl, dbo);
    rc = radb_inject_vl(dbo, vl);
    va_end(vl);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int radb_inject_vl(radbObject *dbo, va_list args) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                     rc = 0,
                            at;
    const char              *x = 0;
#ifdef MYSQL_CLIENT
    MYSQL_BIND              *bindings;
    unsigned long           str_len[100];
    unsigned char           object[1024];
    unsigned char           *O;
    int                     used = 0;
    int                     ss = 0;
    unsigned int            d_uint;
    signed int              d_sint;
    signed long long int    d_lint;
    double                  d_double;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!dbo) return (0);
    at = (int) strlen(dbo->inputs);
#endif
    if (!dbo) return (0);
#ifdef _SQLITE3_H_
    if (dbo->master->dbType == RADB_SQLITE3) {
        for (at = 0; dbo->inputs[at]; at++) {
            switch (dbo->inputs[at])
            {
            case 's':
                x = va_arg(args, const char *);
                rc = sqlite3_bind_text((sqlite3_stmt *) dbo->state, at + 1, x ? x : "", -1, SQLITE_TRANSIENT);
                break;

            case 'u':
                rc = sqlite3_bind_int((sqlite3_stmt *) dbo->state, at + 1, va_arg(args, unsigned int));
                break;

            case 'i':
                rc = sqlite3_bind_int((sqlite3_stmt *) dbo->state, at + 1, va_arg(args, signed int));
                break;

            case 'l':
                rc = sqlite3_bind_int64((sqlite3_stmt *) dbo->state, at + 1, va_arg(args, signed long long int));
                break;

            case 'f':
                rc = sqlite3_bind_double((sqlite3_stmt *) dbo->state, at + 1, va_arg(args, double));
                break;

            default:
                break;
            }

            if (rc) {
                fprintf(stderr, "[RADB] SQLite aborted with code %d at item %u!\r\n", rc, at + 1);
                dbo->state = 0;
            }
        }
    }
#endif
#ifdef MYSQL_CLIENT
    if (dbo->master->dbType == RADB_MYSQL) {
        bindings = (MYSQL_BIND*) calloc(sizeof(MYSQL_BIND), at ? at + 1 : 1);
        dbo->inputBindings = bindings;
        for (at = 0; dbo->inputs[at]; at++) {
            bindings[at].is_null = 0;
            bindings[at].length = 0;
            bindings[at].is_unsigned = 1;
            switch (dbo->inputs[at])
            {
            case 's':
                bindings[at].buffer_type = MYSQL_TYPE_STRING;
                bindings[at].buffer = (void *) va_arg(args, const char *);
#   ifdef RADB_DEBUG
                printf("- row %d (%p): %s\r\n", at + 1, bindings[at].buffer, (char *) bindings[at].buffer);
#   endif
                if (!bindings[at].buffer) bindings[at].is_null_value = 1;
                str_len[at] = (unsigned long) strlen((const char *) bindings[at].buffer);
                bindings[at].buffer_length = str_len[at];
                bindings[at].length = &str_len[at];
                break;

            case 'u':
                O = &(object[used]);
                d_uint = va_arg(args, uint32_t);
                bindings[at].buffer_type = MYSQL_TYPE_LONG;
                bindings[at].buffer = (void *) O;
                str_len[at] = sizeof(uint32_t);
                bindings[at].buffer_length = sizeof(uint32_t);
                bindings[at].length = &str_len[at];
                memcpy(O, &d_uint, str_len[at]);
                used += str_len[at] + 1;
#   ifdef RADB_DEBUG
                printf("- row %d (%p): %u\r\n", at + 1, bindings[at].buffer, *((unsigned int *) bindings[at].buffer));
#   endif
                break;

            case 'i':
                bindings[at].is_unsigned = 0;
                O = &(object[used]);
                d_sint = va_arg(args, signed int);
                ss = sizeof(signed int);
                bindings[at].buffer_type = MYSQL_TYPE_LONG;
                bindings[at].buffer = (void *) O;
                str_len[at] = sizeof(signed int);
                bindings[at].buffer_length = str_len[at];
                bindings[at].length = &str_len[at];
                memcpy(O, &d_uint, str_len[at]);
                used += str_len[at];
                break;

            case 'l':
                bindings[at].is_unsigned = 0;
                bindings[at].buffer_type = MYSQL_TYPE_LONGLONG;
                O = &(object[used]);
                d_lint = va_arg(args, signed long long int);
                bindings[at].buffer_type = MYSQL_TYPE_LONG;
                bindings[at].buffer = (void *) O;
                str_len[at] = sizeof(signed long long int);
                bindings[at].buffer_length = str_len[at];
                bindings[at].length = &str_len[at];
                memcpy(O, &d_uint, str_len[at]);
                used += str_len[at];
                break;

            case 'f':
                bindings[at].is_unsigned = 0;
                bindings[at].buffer_type = MYSQL_TYPE_DOUBLE;
                O = &(object[used]);
                d_double = va_arg(args, double);
                bindings[at].buffer = (void *) O;
                str_len[at] = sizeof(double);
                bindings[at].buffer_length = str_len[at];
                bindings[at].length = &str_len[at];
                memcpy(O, &d_uint, str_len[at]);
                used += str_len[at];
                break;

            default:
                break;
            }
        }

#   ifdef RADB_DEBUG
        printf("[RADB] Binding parameters to statement\r\n");
#   endif
        if (mysql_stmt_bind_param((MYSQL_STMT *) dbo->state, bindings)) {
            dbo->state = 0;
            dbo->status = 0;
            fprintf(stderr, "[RADB] Something went wrong :(\r\n");
            free(bindings);
            return (0);
        }
    }
#endif
    dbo->status = RADB_BOUND;
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
signed int radb_query(radbObject *dbo) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    signed int  rc = 0;
#ifdef MYSQL_CLIENT
    int         numrows = 0;
#endif
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (!dbo) return (0);
#ifdef _SQLITE3_H_
    if (dbo->master->dbType == RADB_SQLITE3) rc = (sqlite3_step((sqlite3_stmt *) dbo->state) == SQLITE_ROW) ? 1 : 0;
#endif
#ifdef MYSQL_CLIENT
    if (dbo->master->dbType == RADB_MYSQL) {
        if (dbo->status == RADB_BOUND) {
            rc = mysql_stmt_execute((MYSQL_STMT *) dbo->state);
            mysql_stmt_store_result((MYSQL_STMT *) dbo->state);
            if (!numrows) numrows = (int) mysql_stmt_num_rows((MYSQL_STMT *) dbo->state);
            if (!rc) numrows = (int) mysql_stmt_affected_rows((MYSQL_STMT *) dbo->state);
            if (rc) {
                printf("Error: %s\r\n", mysql_error((MYSQL *) dbo->db));
                rc = -1;
            }
        } else {
            fprintf(stderr, "[RADB] Couldn't execute the SQL statement: %s!\r\n", mysql_error((MYSQL *) dbo->db));
            rc = -1;
        }

        rc = numrows;
    }
#endif
    dbo->status = RADB_EXECUTED;
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void radb_prepare_result(radbObject *dbo) {
    if (!dbo) return;
    dbo->result = 0;
#ifdef MYSQL_CLIENT
    if (dbo->master->dbType == RADB_MYSQL) {

        /*~~~~~~~~~~~~~~~~~~~~~~*/
        MYSQL_RES       *meta;
        MYSQL_BIND      *bindings;
        unsigned int    i;
        /*~~~~~~~~~~~~~~~~~~~~~~*/

        meta = mysql_stmt_result_metadata((MYSQL_STMT *) dbo->state);
        if (meta) {
            dbo->result = (radbResult *) malloc(sizeof(radbResult));
            dbo->result->items = meta->field_count;
            dbo->result->column = (radbItem*) calloc(sizeof(radbItem), dbo->result->items ? dbo->result->items : 1);
            bindings = (MYSQL_BIND*) calloc(sizeof(MYSQL_BIND), dbo->result->items);
            for (i = 0; i < dbo->result->items; i++) {
                bindings[i].buffer = (void *) dbo->result->column[i].data.string;
                dbo->result->column[i].type = (meta->fields[i].type >= MYSQL_TYPE_VARCHAR) ? 1 : 2;
                bindings[i].buffer_type = meta->fields[i].type;
                bindings[i].buffer_length = meta->fields[i].length + 1;
            }

            mysql_free_result(meta);
            dbo->result->bindings = bindings;
        }
    }
#endif
#ifdef _SQLITE3_H_
    if (dbo->master->dbType == RADB_SQLITE3) {

        /*~~~~~~*/
        int count;
        /*~~~~~~*/

        count = sqlite3_column_count((sqlite3_stmt *) dbo->state);
        if (!count) return;
        dbo->result = (radbResult*) malloc(sizeof(radbResult));
        dbo->result->column = (radbItem*) calloc(sizeof(radbItem), count);
        dbo->result->items = count;
        dbo->result->bindings = 0;
    }
#endif
    dbo->status = RADB_FETCH;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void radb_free_result(radbResult *result) {
    if (!result) return;
#ifdef RADB_DEBUG
    printf("freeing up result data\r\n");
#endif
    if (result->column) free(result->column);
    if (result->bindings) free(result->bindings);
    free(result);
#ifdef RADB_DEBUG
    printf("done!!\r\n");
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
signed int radb_run(radbMaster *radbm, const char *statement) {

    /*~~~~~~~~~~~~~~~~~*/
    radbObject  *dbo = 0;
    signed int  rc = 0;
    /*~~~~~~~~~~~~~~~~~*/

#ifdef RADB_DEBUG
    printf("radb_run: %s\r\n", statement);
    if (!radbm) printf("Error: dbm is (null)\r\n");
#endif
    if (!radbm) return (-1);
    dbo = radb_init_object(radbm);
#ifdef MYSQL_CLIENT
    if (radbm->dbType == RADB_MYSQL) {
        dbo->state = mysql_stmt_init((MYSQL *) dbo->db);
        if (!dbo->state) {
            printf("radb_run: Couldn't init a statement!\r\n");
            rc = -1;
        } else {
            rc = mysql_stmt_prepare((MYSQL_STMT *) dbo->state, statement, (unsigned long) strlen(statement));
            if (rc) {
                printf("radb_run Error: %s\r\n", mysql_error((MYSQL *) dbo->db));
                rc = -1;
            } else dbo->status = RADB_BOUND;
#   ifdef RADB_DEBUG
            printf("Statement prepared\n");
#   endif
        }
    }
#endif
#ifdef _SQLITE3_H_
    if (radbm->dbType == RADB_SQLITE3) {
        rc = sqlite3_prepare_v2((sqlite3 *) dbo->db, statement, -1, (sqlite3_stmt **) &dbo->state, NULL);
    }
#endif
    rc = radb_query(dbo);
    radb_cleanup(dbo);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int radb_run_inject(radbMaster *radbm, const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~*/
    va_list     vl;
    radbObject  *dbo = 0;
    int         rc = 0;
    /*~~~~~~~~~~~~~~~~~*/

    if (!radbm) return (-1);
#ifdef RADB_DEBUG
    printf("radb_run_inject: %s\r\n", statement);
#endif
    va_start(vl, statement);
    dbo = radb_prepare_vl(radbm, statement, vl);
    va_end(vl);
    rc = radb_query(dbo);
    radb_cleanup(dbo);
    return (rc);
}

/*$3
 =======================================================================================================================
    MySQL functions
 =======================================================================================================================
 */

#ifdef MYSQL_CLIENT /* Only compile if mysql support is enabled */

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbMaster *radb_init_mysql(unsigned threads, const char *host, const char *user, const char *pass, const char *db, unsigned port) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned    i,
                ok = 1;
    my_bool     yes = 1;
    MYSQL       *m;
    radbMaster  *radbm = (radbMaster*) malloc(sizeof(radbMaster));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    radbm->dbType = RADB_MYSQL;
    radbm->pool.count = threads;
    radbm->pool.children = (radbChild*) calloc(threads, sizeof(radbChild));
    for (i = 0; i < threads; i++) {
        radbm->pool.children[i].handle = mysql_init(0);
        m = (MYSQL *) radbm->pool.children[i].handle;
        mysql_options(m, MYSQL_OPT_RECONNECT, &yes);
        if (!mysql_real_connect(m, host, user, pass, db, port, 0, 0)) {
            fprintf(stderr, "Failed to connect to database: Error: %s", mysql_error(m));
            ok = 0;
            break;
        }
    }

    if (!ok) return (0);
    return (radbm);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void *radb_get_handle_mysql(radbPool *pool) {

    /*~~~~~~~~~~~~~~~~~~*/
    unsigned int    i,
                    x = 5;
    /*~~~~~~~~~~~~~~~~~~*/

    while (x != 0) {
        for (i = 0; i < pool->count; i++) {
            if (pool->children[i].inUse == 0) {
                pool->children[i].inUse = 1;
                return (pool->children[i].handle);
            }
        }

        x--;
    }

    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void radb_release_handle_mysql(radbPool *pool, void *handle) {

    /*~~~~~~~~~~~~~~*/
    unsigned int    i;
    /*~~~~~~~~~~~~~~*/

    for (i = 0; i < pool->count; i++) {
        if (pool->children[i].handle == handle) {
            pool->children[i].inUse = 0;
            break;
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbResult *radb_fetch_row_mysql(radbObject *dbo) {

    /*~~~~~~~~~~~~~~~~~~~*/
    int             rc = 0;
    unsigned int    i = 0;
    /*~~~~~~~~~~~~~~~~~~~*/

    if (dbo->status <= RADB_BOUND) radb_query(dbo);
    if (dbo->status <= RADB_EXECUTED) {
        radb_prepare_result(dbo);
        mysql_stmt_bind_result((MYSQL_STMT *) dbo->state, (MYSQL_BIND *) dbo->result->bindings);
        mysql_stmt_store_result((MYSQL_STMT *) dbo->state);
    }

    rc = mysql_stmt_fetch((MYSQL_STMT *) dbo->state);
    if (rc) {
        if (rc == MYSQL_DATA_TRUNCATED) {

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            int         x = 0;
            MYSQL_BIND  *bind = (MYSQL_BIND *) dbo->result->bindings;
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            for (i = 0; i < dbo->result->items; i++) {
                if (bind[i].error_value) {
                    fprintf(stderr, "[RADB] MySQL says row %d was truncated\r\n", i + 1);
                    x++;
                }
            }

            fprintf(stderr, "[RADB] Found %d truncated fields\r\n", x);
        }

        printf("No more rows\r\n");
        return (0);
    }

    printf("found a row!\r\n");
    return (dbo->result);
}
#endif

/*$I0 */
#include "radb.h"
#ifdef _SQLITE3_H_

/*$3
 =======================================================================================================================
    SQLite3 functions
 =======================================================================================================================
 */

radbMaster *radb_init_sqlite(const char *file) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    radbMaster  *radbm = (radbMaster*) malloc(sizeof(radbMaster));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    radbm->dbType = RADB_SQLITE3;
    radbm->pool.count = 0;
    if (sqlite3_open(file, (sqlite3 **) &radbm->handle)) {
        fprintf(stderr, "[RADB] Couldn't open %s: %s\r\n", file, sqlite3_errmsg((sqlite3 *) radbm->handle));
        return (0);
    }

    return (radbm);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbResult *radb_fetch_row_sqlite(radbObject *dbo) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    int             rc = -1,
                    l;
    unsigned int    i = 0;
    radbResult      *res;
    /*~~~~~~~~~~~~~~~~~~~~*/

    if (dbo->status == RADB_FETCH) rc = sqlite3_step((sqlite3_stmt *) dbo->state);
    if (dbo->status <= RADB_BOUND) rc = (radb_query(dbo) == 1) ? SQLITE_ROW : 0;
    if (dbo->status <= RADB_EXECUTED) radb_prepare_result(dbo);
    res = dbo->result;
    if (rc != SQLITE_ROW) return (0);
    for (i = 0; i < res->items; i++) {
        l = sqlite3_column_bytes((sqlite3_stmt *) dbo->state, i);
        memset(res->column[i].data.string, 0, l + 1);
        res->column[i].type = 2;
        switch (sqlite3_column_type((sqlite3_stmt *) dbo->state, i))
        {
        case SQLITE_TEXT:
            res->column[i].type = 1;
            memcpy(res->column[i].data.string, sqlite3_column_text((sqlite3_stmt *) dbo->state, i), l);
            break;

        case SQLITE_INTEGER:
            res->column[i].data.int64 = sqlite3_column_int64((sqlite3_stmt *) dbo->state, i);
            break;

        case SQLITE_FLOAT:
            res->column[i].data._double = sqlite3_column_double((sqlite3_stmt *) dbo->state, i);
            break;

        default:
            break;
        }
    }

    return (res);
}
#endif
#ifdef __cplusplus

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbo::radbo(radbMaster *db) {
    this->dbo = radb_init_object(db);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbo::radbo(void)
{ };

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 void radbo::cleanup(void) {
    radb_cleanup(this->dbo);
    this->dbo = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radbo::~radbo(void) {
    if (this->dbo) radb_cleanup(this->dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 int radbo::query(void) {
    return (radb_query(this->dbo));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 radbResult *radbo::fetch_row(void) {
    return (radb_fetch_row(this->dbo));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 int radbo::inject(...) {

    /*~~~~~~~*/
    int     rc;
    va_list vl;
    /*~~~~~~~*/

    va_start(vl, dbo);
    rc = radb_inject_vl(this->dbo, vl);
    va_end(vl);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 void radb::disconnect(void) {
    if (this->dbm) radb_close(this->dbm);
    this->dbm = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
radb::~radb(void) {
    if (this->dbm) radb_close(this->dbm);
    this->dbm = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 int radb::run(const char *statement) {
    return (radb_run(this->dbm, statement));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 int radb::run_inject(const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~*/
    va_list     vl;
    radbObject  *dbo = 0;
    int         rc = 0;
    /*~~~~~~~~~~~~~~~~~*/

#   ifdef RADB_DEBUG
    printf("radb_run_inject: %s\r\n", statement);
#   endif
    va_start(vl, statement);
    dbo = radb_prepare_vl(this->dbm, statement, vl);
    va_end(vl);
    rc = radb_query(dbo);
    radb_cleanup(dbo);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
 radbo *radb::prepare(const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~~~~~~~*/
    va_list vl;
    radbo   *dbo = new radbo();
    /*~~~~~~~~~~~~~~~~~~~~~~~*/

    va_start(vl, statement);
    dbo->dbo = radb_prepare_vl(dbm, statement, vl);
    va_end(vl);
    return (dbo);
}
 
 void radb::init_sqlite(const char* filename) {
     this->dbm = radb_init_sqlite(filename);
     return;
 }
#endif
