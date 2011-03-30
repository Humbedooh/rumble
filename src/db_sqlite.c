/*$I0 */
#include "database.h"
extern masterHandle *rumble_database_master_handle;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_load_sqlite(masterHandle *master, FILE *runlog) {

    /*~~~~~~~~~~~~~~~~~~~*/
    char    dbpath[1024];
    char    mailpath[1024];
    int     rc;
    FILE    *ftmp;
    /*~~~~~~~~~~~~~~~~~~~*/

    sprintf(dbpath, "%s/rumble.sqlite", rumble_config_str(master, "datafolder"));
    sprintf(mailpath, "%s/mail.sqlite", rumble_config_str(master, "datafolder"));
    ftmp = fopen(dbpath, "r");
    if (!ftmp) {
        sprintf(dbpath, "%s/%s/rumble.sqlite", rrdict(master->_core.conf, "execpath"), rumble_config_str(master, "datafolder"));
        sprintf(mailpath, "%s/%s/mail.sqlite", rrdict(master->_core.conf, "execpath"), rumble_config_str(master, "datafolder"));
    } else fclose(ftmp);
    printf("%-48s", "Loading database...");
    statusLog("Loading database");

    /* Domains and accounts */
    if (sqlite3_open(dbpath, (sqlite3 **) &master->_core.db)) {
        statusLog("ERROR: Can't open database <%s>: %s\r\n", dbpath, sqlite3_errmsg((sqlite3 *) master->_core.db));
        fprintf(stderr, "ERROR: Can't open database <%s>: %s", dbpath, sqlite3_errmsg((sqlite3 *) master->_core.db));
        exit(EXIT_FAILURE);
    }

    /* Letters */
    if (sqlite3_open(mailpath, (sqlite3 **) &master->_core.mail)) {
        statusLog("ERROR: Can't open database <%s>: %s\r\n", mailpath, sqlite3_errmsg((sqlite3 *) master->_core.mail));
        fprintf(stderr, "ERROR: Can't open database <%s>: %s", mailpath, sqlite3_errmsg((sqlite3 *) master->_core.mail));
        exit(EXIT_FAILURE);
    }

    /*$2
     -------------------------------------------------------------------------------------------------------------------
        Check if the tables exists or not
     -------------------------------------------------------------------------------------------------------------------
     */

    rc = rumble_database_do(0, "PRAGMA table_info (queue)");
    if (!rc) {
        printf("[OK]\r\n");
        printf("%-48s", "Setting up tables...");
        statusLog("New installation, creating DB");
        rc = rumble_database_do(0,
                                "CREATE TABLE \"domains\" (\"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"domain\" VARCHAR NOT NULL , \"storagepath\" VARCHAR);");
        rc = rumble_database_do(0,
                                "CREATE TABLE \"accounts\" (\"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"domain\" VARCHAR NOT NULL , \"user\" VARCHAR, \"password\" CHAR(64), \"type\" CHAR(5) NOT NULL  DEFAULT mbox, \"arg\" VARCHAR);");
        rc = rumble_database_do(0,
                                "CREATE TABLE \"folders\" (\"uid\" INTEGER NOT NULL  DEFAULT 0, \"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"name\" VARCHAR NOT NULL , \"subscribed\" BOOL NOT NULL  DEFAULT false);");
        rc = rumble_database_do(0,
                                "CREATE TABLE \"mbox\" (\"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"uid\" INTEGER NOT NULL , \"fid\" VARCHAR NOT NULL , \"size\" INTEGER NOT NULL , \"delivered\" INTEGER DEFAULT (strftime('%%s', 'now')), \"folder\" INTEGER NOT NULL DEFAULT 0, \"flags\" INTEGER NOT NULL DEFAULT 1 );");
        rc = rumble_database_do(0,
                                "CREATE TABLE \"queue\" (\"id\" INTEGER PRIMARY KEY  NOT NULL ,\"time\" INTEGER NOT NULL  DEFAULT (STRFTIME('%%s','now')) ,\"loops\" INTEGER NOT NULL  DEFAULT (0) ,\"fid\" VARCHAR NOT NULL ,\"sender\" VARCHAR NOT NULL ,\"recipient\" VARCHAR NOT NULL ,\"flags\" INTEGER NOT NULL  DEFAULT (0) );");
        if (!rc) printf("[OK]\r\n");
        else {
            fprintf(runlog, "[%s]\r\n", sqlite3_errmsg((sqlite3 *) master->_core.db));
            fflush(runlog);
            printf("[%s]\r\n", sqlite3_errmsg((sqlite3 *) master->_core.db));
        }
    } else printf("[OK]\r\n");
    statusLog("Database successfully initialized");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_prepare_sqlite(dbObject *dbo, const char *statement, va_list vl) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    char        *sql,
                b;
    const char  *p,
                *op;
    char        injects[32];
    int         rc;
    size_t      len = 0,
                strl = 0;
    int         at = 0;
    /*~~~~~~~~~~~~~~~~~~~~*/

    printf("[RDB] Preparing: %s\r\n", statement);
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
    rc = sqlite3_prepare_v2((sqlite3 *) rumble_database_master_handle->_core.db, sql, -1, (sqlite3_stmt **) &dbo->state, NULL);
    free(sql);
    if (rc != SQLITE_OK) {
        dbo->state = 0;
        return;
    }

    for (at = 0; injects[at] != 0; at++) {
        switch (injects[at])
        {
        case 's':   rc = sqlite3_bind_text((sqlite3_stmt *) dbo->state, at + 1, va_arg(vl, const char *), -1, SQLITE_TRANSIENT); break;
        case 'u':   rc = sqlite3_bind_int((sqlite3_stmt *) dbo->state, at + 1, va_arg(vl, unsigned int)); break;
        case 'i':   rc = sqlite3_bind_int((sqlite3_stmt *) dbo->state, at + 1, va_arg(vl, signed int)); break;
        case 'l':   rc = sqlite3_bind_int64((sqlite3_stmt *) dbo->state, at + 1, va_arg(vl, signed long long int)); break;
        case 'f':   rc = sqlite3_bind_double((sqlite3_stmt *) dbo->state, at + 1, va_arg(vl, double)); break;
        default:    break;
        }

        if (rc != SQLITE_OK) {
            dbo->state = 0;
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dbResult *rumble_database_fetch_row_sqlite(void *state) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int                     count,
                            i,
                            l;
    signed long long int    *p;
    double                  *d;
    dbObject                *dbo = (dbObject *) state;
    dbResult                *res;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("[RDB] Fetching a row from state %p\r\n", dbo->state);
    count = sqlite3_column_count((sqlite3_stmt *) dbo->state);
    if (!count) return (0);
    res = malloc(sizeof(dbResult));
    res->column = malloc(count * sizeof(dbItem));
    res->items = count;
    for (i = 0; i < count; i++) {
        l = sqlite3_column_bytes((sqlite3_stmt *) dbo->state, i);
        res->column[i].data = calloc(1, l + 1);
        res->column[i].size = l;
        res->column[i].type = 2;
        switch (sqlite3_column_type((sqlite3_stmt *) dbo->state, i))
        {
        case SQLITE_TEXT:
            res->column[i].type = 1;
            memcpy(res->column[i].data, sqlite3_column_text((sqlite3_stmt *) dbo->state, i), l);
            break;

        case SQLITE_INTEGER:
            p = res->column[i].data;
            *p = sqlite3_column_int64((sqlite3_stmt *) dbo->state, i);
            break;

        case SQLITE_FLOAT:
            d = res->column[i].data;
            *d = sqlite3_column_double((sqlite3_stmt *) dbo->state, i);
            break;

        default:
            break;
        }
    }

    return (res);
}
