/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
masterHandle    *rumble_database_master_handle = 0;
int             rumble_database_engine = RUMBLE_SQLITE3;

/*
 =======================================================================================================================
    Database constructors and wrappers
 =======================================================================================================================
 */
void rumble_database_load(masterHandle *master, FILE *runlog) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char  *dbe = rrdict(master->_core.conf, "databaseengine");
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!strcmp(dbe, "mysql")) rumble_database_engine = RUMBLE_MYSQL;
    if (!strcmp(dbe, "postgresql")) rumble_database_engine = RUMBLE_POSTGRESQL;
    if (rumble_database_engine == RUMBLE_SQLITE3) rumble_database_load_sqlite(master, runlog);
#ifdef MYSQL_CLIENT
    if (rumble_database_engine == RUMBLE_MYSQL) rumble_database_load_mysql(master, runlog);
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dbResult *rumble_database_step(dbObject *dbo) {
    printf("[RDB] Stepping\r\n");
    if (rumble_database_engine == RUMBLE_SQLITE3) {
        if (sqlite3_step((sqlite3_stmt *) dbo->state) == SQLITE_ROW) {
            printf("[RDB] SQLITE says there be dragons in %p!\r\n", dbo->state);
            return (rumble_database_fetch_row_sqlite(dbo));
        }
    }

#ifdef MYSQL_CLIENT
    else if (rumble_database_engine == RUMBLE_MYSQL) {
        if (dbo->result == 0) {
            dbo->result = 1;
            mysql_stmt_execute((MYSQL_STMT *) dbo->state);
        }

        return (rumble_database_fetch_row_mysql(dbo->state));
    }
#endif
    return (0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_cleanup(dbObject *dbo) {
    if (rumble_database_engine == RUMBLE_SQLITE3) sqlite3_finalize((sqlite3_stmt *) dbo->state);
#ifdef MYSQL_CLIENT
    else if (rumble_database_engine == RUMBLE_MYSQL)
        mysql_stmt_close((MYSQL_STMT *) dbo->state);
    free((dbObject *) dbo);
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int rumble_database_do(void *db, const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    va_list     args;
    dbObject    *dbo = calloc(1, sizeof(dbObject));
    int         rc = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("[RDB] Running RDB->do\r\n");
    dbo->db = db;
    dbo->result = 0;
    va_start(args, statement);
    if (rumble_database_engine == RUMBLE_SQLITE3) {
        dbo->db = (db != 0) ? db : rumble_database_master_handle->_core.db;
        rumble_database_prepare_sqlite(dbo, statement, args);
        printf("[RDB] Stepping\r\n");
        rc = (sqlite3_step((sqlite3_stmt *) dbo->state) == SQLITE_ROW) ? 1 : 0;
    }

#ifdef MYSQL_CLIENT
    else if (rumble_database_engine == RUMBLE_MYSQL) {

        /*
         * dbo->db = (db != 0) ? db : rumble_database_get_handle_mysql(rumble_database_master_handle->_core.db);
         */
        rumble_database_prepare_mysql(dbo, statement, args);
        printf("[RDB] Stepping\r\n");
        mysql_stmt_execute((MYSQL_STMT *) dbo->state);
        rc = mysql_stmt_affected_rows((MYSQL_STMT *) dbo->state);
    }
#endif
    va_end(args);
    rumble_database_cleanup(dbo);
    printf("[RDB] Step returned %d\r\n", rc);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dbObject *rumble_database_prepare(void *db, const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    va_list     args;
    dbObject    *dbo = calloc(1, sizeof(dbObject));
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    printf("[RDB] pre-preparation of: %s\r\n", statement);
    dbo->db = db;
    dbo->result = 0;
    va_start(args, statement);
    if (rumble_database_engine == RUMBLE_SQLITE3) rumble_database_prepare_sqlite(dbo, statement, args);
#ifdef MYSQL_CLIENT
    else if (rumble_database_engine == RUMBLE_MYSQL)
        rumble_database_prepare_mysql(dbo, statement, args);
#endif
    else {
        printf("[RDB] PANIC: I don't know what database we're using!!!\r\n");
    }

    va_end(args);
    return (dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_free_account(rumble_mailbox *user) {
    if (!user) return;
    if (user->arg) free(user->arg);
    if (user->user) free(user->user);
    if (user->hash) free(user->hash);
    if (user->domain) {
        if (user->domain->name) free(user->domain->name);
        if (user->domain->path) free(user->domain->path);
        free(user->domain);
    }

    user->arg = 0;
    user->domain = 0;
    user->user = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_account_exists(sessionHandle *session, const char *user, const char *domain) {

    /*~~~*/
    int rc;
    /*~~~*/

    rc = rumble_database_do(0, "SELECT 1 FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1", domain, user);
    return (rc == RUMBLE_DB_RESULT) ? 1 : 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_account_exists_raw(const char *user, const char *domain) {

    /*~~~*/
    int rc;
    /*~~~*/

    rc = rumble_database_do(0, "SELECT 1 FROM accounts WHERE domain = %s AND user = %s ORDER BY LENGTH(user) DESC LIMIT 1", domain, user);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_mailbox *rumble_account_data(uint32_t uid, const char *user, const char *domain) {

    /*~~~~~~~~~~~~~~~~~*/
    dbObject        *dbo;
    dbResult        *dbr;
    char            *tmp;
    rumble_mailbox  *acc;
    /*~~~~~~~~~~~~~~~~~*/

    if (uid) {
        dbo = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                      "SELECT id, domain, user, password, type, arg FROM accounts WHERE id = %u LIMIT 1", uid);
    } else {
        dbo = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                      "SELECT id, domain, user, password, type, arg FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",
                                  domain ? domain : "", user ? user : "");
    }

    dbr = rumble_database_step(dbo);
    acc = NULL;
    if (dbr) {
        acc = (rumble_mailbox *) malloc(sizeof(rumble_mailbox));
        if (!acc) merror();

        /* Account UID */
        acc->uid = rdbas(uint32_t, dbr->column[0].data);

        /* Account Domain struct */
        acc->domain = rumble_domain_copy((char *) dbr->column[1].data);

        /* Account Username */
        acc->user = strclone(dbr->column[2].data);

        /* Password (hashed) */
        acc->hash = strclone(dbr->column[3].data);

        /* Account type */
        tmp = strclone(dbr->column[4].data);
        rumble_string_lower(tmp);
        acc->type = RUMBLE_MTYPE_MBOX;
        if (!strcmp(tmp, "alias")) acc->type = RUMBLE_MTYPE_ALIAS;
        else if (!strcmp(tmp, "mod"))
            acc->type = RUMBLE_MTYPE_MOD;
        else if (!strcmp(tmp, "feed"))
            acc->type = RUMBLE_MTYPE_FEED;
        else if (!strcmp(tmp, "relay"))
            acc->type = RUMBLE_MTYPE_FEED;
        free(tmp);

        /* Account args */
        acc->arg = strclone(dbr->column[5].data);
        rdbfree(dbr);
    }

    rumble_database_cleanup(dbo);
    return (acc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_mailbox *rumble_account_data_auth(uint32_t uid, const char *user, const char *domain, const char *pass) {

    /*~~~~~~~~~~~~~~~~~~*/
    rumble_mailbox  *acc;
    char            *hash;
    /*~~~~~~~~~~~~~~~~~~*/

    acc = rumble_account_data(0, user, domain);
    if (acc) {
        hash = rumble_sha256((const unsigned char *) pass);
        if (!strcmp(hash, acc->hash)) return (acc);
        rumble_free_account(acc);
        acc = 0;
    }

    return (acc);
}

/*
 =======================================================================================================================
    rumble_domain_exists: Checks if a domain exists in the database. Returns 1 if true, 0 if false.
 =======================================================================================================================
 */
uint32_t rumble_domain_exists(const char *domain) {

    /*~~~~~~~~~~~~~~~~~*/
    uint32_t        rc;
    rumble_domain   *dmn;
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~*/

    rc = 0;
    rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
    foreach((rumble_domain *), dmn, rumble_database_master_handle->domains.list, iter) {
        if (!strcmp(dmn->name, domain)) {
            rc = 1;
            break;
        }
    }

    rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
    return (rc);
}

/*
 =======================================================================================================================
    rumble_domain_copy: Returns a copy of the domain info
 =======================================================================================================================
 */
rumble_domain *rumble_domain_copy(const char *domain) {

    /*~~~~~~~~~~~~~~~~~*/
    rumble_domain   *dmn,
                    *rc;
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~*/

    rc = 0;
    rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
    foreach((rumble_domain *), dmn, rumble_database_master_handle->domains.list, iter) {
        if (!strcmp(dmn->name, domain)) {
            rc = (rumble_domain *) malloc(sizeof(rumble_domain));
            rc->name = (char *) calloc(1, strlen(dmn->name) + 1);
            rc->path = (char *) calloc(1, strlen(dmn->path) + 1);
            strcpy(rc->name, dmn->name);
            strcpy(rc->path, dmn->path);
            rc->id = dmn->id;
            break;
        }
    }

    rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
    return (rc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
cvector *rumble_domains_list(void) {

    /*~~~~~~~~~~~~~~~~~~*/
    rumble_domain   *dmn,
                    *rc;
    d_iterator      iter;
    cvector         *cvec;
    /*~~~~~~~~~~~~~~~~~~*/

    cvec = cvector_init();
    rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
    foreach((rumble_domain *), dmn, rumble_database_master_handle->domains.list, iter) {
        rc = (rumble_domain *) malloc(sizeof(rumble_domain));
        rc->id = 0;
        rc->name = (char *) calloc(1, strlen(dmn->name) + 1);
        rc->path = (char *) calloc(1, strlen(dmn->path) + 1);
        strcpy(rc->name, dmn->name);
        strcpy(rc->path, dmn->path);
        rc->id = dmn->id;
        cvector_add(cvec, rc);
    }

    rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
    return (cvec);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
cvector *rumble_database_accounts_list(const char *domain) {

    /*~~~~~~~~~~~~~~~~~~*/
    dbObject        *dbo;
    dbResult        *dbr;
    cvector         *cvec;
    rumble_mailbox  *acc;
    char            *tmp;
    /*~~~~~~~~~~~~~~~~~~*/

    cvec = cvector_init();
    if (rumble_domain_exists(domain)) {
        dbo = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                      "SELECT id, user, password, type, arg FROM accounts WHERE domain = %s", domain);
        acc = NULL;
        while ((dbr = rumble_database_step(dbo))) {
            acc = (rumble_mailbox *) malloc(sizeof(rumble_mailbox));
            if (!acc) merror();

            /* Account UID */
            acc->uid = rdbas(uint32_t, dbr->column[0].data);

            /* Account Username */
            acc->user = strclone(dbr->column[1].data);

            /* Password (hashed) */
            acc->hash = strclone(dbr->column[2].data);

            /* Account type */
            tmp = strclone(dbr->column[3].data);
            rumble_string_lower(tmp);
            acc->type = RUMBLE_MTYPE_MBOX;
            if (!strcmp(tmp, "alias")) acc->type = RUMBLE_MTYPE_ALIAS;
            else if (!strcmp(tmp, "mod"))
                acc->type = RUMBLE_MTYPE_MOD;
            else if (!strcmp(tmp, "feed"))
                acc->type = RUMBLE_MTYPE_FEED;
            else if (!strcmp(tmp, "relay"))
                acc->type = RUMBLE_MTYPE_FEED;
            free(tmp);

            /* Account args */
            acc->arg = strclone(dbr->column[4].data);
            acc->domain = 0;
            cvector_add(cvec, acc);
            rdbfree(dbr);
        }

        rumble_database_cleanup(dbo);
    }

    return (cvec);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_accounts_free(cvector *accounts) {

    /*~~~~~~~~~~~~~~~~~~*/
    c_iterator      citer;
    rumble_mailbox  *acc;
    /*~~~~~~~~~~~~~~~~~~*/

    cforeach((rumble_mailbox *), acc, accounts, citer) {
        if (acc->hash) free(acc->hash);
        if (acc->user) free(acc->user);
        if (acc->arg) free(acc->arg);
        free(acc);
    }

    cvector_destroy(accounts);
}

/*
 =======================================================================================================================
    Internal database functions (not for use by modules) ;
    rumble_database_update_domains: Updates the list of domains from the db
 =======================================================================================================================
 */
void rumble_database_update_domains(void) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    dbObject        *dbo;
    dbResult        *dbr;
    rumble_domain   *domain;
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    printf("loading domain list\r\n");

    /* Clean up the old list */
    rumble_rw_start_write(rumble_database_master_handle->domains.rrw);
    foreach((rumble_domain *), domain, rumble_database_master_handle->domains.list, iter) {
        free(domain->name);
        free(domain->path);
        free(domain);
    }

    dvector_flush(rumble_database_master_handle->domains.list);
    dbo = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, domain, storagepath FROM domains WHERE 1");
    while ((dbr = rumble_database_step(dbo))) {
        domain = (rumble_domain *) malloc(sizeof(rumble_domain));
        printf("Got a hit with %d cols!\r\n", dbr->items);

        /* Domain ID */
        domain->id = rdbas(uint32_t, dbr->column[0].data);

        /* Domain name */
        domain->name = strclone(dbr->column[1].data);
        printf("Found: %s\r\b", domain->name);

        /* Optional domain specific storage path */
        domain->path = strclone(dbr->column[2].data);
        dvector_add(rumble_database_master_handle->domains.list, domain);
        rdbfree(dbr);
    }

    rumble_rw_stop_write(rumble_database_master_handle->domains.rrw);
    rumble_database_cleanup(dbo);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_database_free_result(dbResult *result) {

    /*~~~~~~~~~~*/
    unsigned    i;
    /*~~~~~~~~~~*/

    for (i = 0; i < result->items; i++) {
        free(result->column[i].data);
    }

    if (result->column) free(result->column);
    free(result);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
dbResult *rumble_database_fetch_row(void *state) {
    if (rumble_database_engine == RUMBLE_SQLITE3) return (rumble_database_fetch_row_sqlite(state));
#ifdef MYSQL_CLIENT
    if (rumble_database_engine == RUMBLE_MYSQL) return (rumble_database_fetch_row_mysql(state));
#endif
    return (0);
}
