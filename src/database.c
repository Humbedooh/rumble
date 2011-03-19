/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
masterHandle    *rumble_database_master_handle = 0;

/*
 =======================================================================================================================
    Database constructors and wrappers
 =======================================================================================================================
 */
void rumble_database_load(masterHandle *master, FILE* runlog) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    dbpath[1024];
    char    mailpath[1024];
    void    *state;
    int     rc;
	FILE* ftmp;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sprintf(dbpath, "%s/rumble.sqlite", rumble_config_str(master, "datafolder"));
    sprintf(mailpath, "%s/mail.sqlite", rumble_config_str(master, "datafolder"));
	ftmp = fopen(dbpath, "r");
	if (!ftmp) {
            sprintf(dbpath, "%s/%s/rumble.sqlite", rrdict(master->_core.conf, "execpath"), rumble_config_str(master, "datafolder"));
            sprintf(mailpath, "%s/%s/mail.sqlite", rrdict(master->_core.conf, "execpath"), rumble_config_str(master, "datafolder"));
        }
	else fclose(ftmp);
    
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

    state = rumble_database_prepare(0, "PRAGMA table_info (queue)");
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    if (rc != SQLITE_ROW) {
        printf("[OK]\r\n");
        printf("%-48s", "Setting up tables...");
        statusLog("New installation, creating DB");
        state = rumble_database_prepare(0,
                                        "CREATE TABLE \"domains\" (\"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"domain\" VARCHAR NOT NULL , \"storagepath\" VARCHAR);");
        rc = (rumble_database_run(state) == SQLITE_DONE) ? SQLITE_DONE : SQLITE_ERROR;
        rumble_database_cleanup(state);
        state = rumble_database_prepare(0,
                                        "CREATE TABLE \"accounts\" (\"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"domain\" VARCHAR NOT NULL , \"user\" VARCHAR, \"password\" CHAR(64), \"type\" CHAR(5) NOT NULL  DEFAULT mbox, \"arg\" VARCHAR);");
        rc = (rumble_database_run(state) == SQLITE_DONE) ? SQLITE_DONE : SQLITE_ERROR;
        rumble_database_cleanup(state);
        state = rumble_database_prepare(0,
                                        "CREATE TABLE \"folders\" (\"uid\" INTEGER NOT NULL  DEFAULT 0, \"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"name\" VARCHAR NOT NULL , \"subscribed\" BOOL NOT NULL  DEFAULT false);");
        rc = (rumble_database_run(state) == SQLITE_DONE) ? SQLITE_DONE : SQLITE_ERROR;
        rumble_database_cleanup(state);
        state = rumble_database_prepare(0,
                                        "CREATE TABLE \"mbox\" (\"id\" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL  UNIQUE , \"uid\" INTEGER NOT NULL , \"fid\" VARCHAR NOT NULL , \"size\" INTEGER NOT NULL , \"delivered\" INTEGER DEFAULT (strftime('%%s', 'now')), \"folder\" INTEGER NOT NULL DEFAULT 0, \"flags\" INTEGER NOT NULL DEFAULT 1 );");
        rc = (rumble_database_run(state) == SQLITE_DONE) ? SQLITE_DONE : SQLITE_ERROR;
        rumble_database_cleanup(state);
        state = rumble_database_prepare(0,
                                        "CREATE TABLE \"queue\" (\"id\" INTEGER PRIMARY KEY  NOT NULL ,\"time\" INTEGER NOT NULL  DEFAULT (STRFTIME('%%s','now')) ,\"loops\" INTEGER NOT NULL  DEFAULT (0) ,\"fid\" VARCHAR NOT NULL ,\"sender\" VARCHAR NOT NULL ,\"recipient\" VARCHAR NOT NULL ,\"flags\" INTEGER NOT NULL  DEFAULT (0) );");
        rc = (rumble_database_run(state) == SQLITE_DONE) ? SQLITE_DONE : SQLITE_ERROR;
        rumble_database_cleanup(state);
        if (rc == SQLITE_DONE) printf("[OK]\r\n");
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
    Wrapper for the SQL prepare statement
 =======================================================================================================================
 */
void *rumble_database_prepare(void *db, const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    char        *sql,
                b;
    const char  *p,
                *op;
    char        injects[32];
    void        *returnObject;
    va_list     vl;
    int         rc;
    size_t      len = 0,
                strl = 0;
    int         at = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    memset(injects, 0, 32);
    sql = (char *) calloc(1, 2048);
    if (!sql) merror();
    op = statement;
    for (p = strchr(statement, '%'); p != NULL; p = strchr(op, '%')) {
        strl = strlen(op) - strlen(p);
        strncpy((char *) (sql + len), op, strl);
        len += strl;
        sscanf((const char *) p, "%%%c", &b);
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

    strl = strlen(op);
    strncpy((char *) (sql + len), op, strl);
#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
    rc = sqlite3_prepare_v2((sqlite3 *) rumble_database_master_handle->_core.db, sql, -1, (sqlite3_stmt **) &returnObject, NULL);
    free(sql);
    if (rc != SQLITE_OK) return (0);
#endif
    va_start(vl, statement);
    for (at = 0; injects[at] != 0; at++) {
        switch (injects[at])
        {
        case 's':
#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
            rc = sqlite3_bind_text((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, const char *), -1, SQLITE_TRANSIENT);
#endif
            break;

        case 'u':
#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
            rc = sqlite3_bind_int((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, unsigned int));
#endif
            break;

        case 'i':
#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
            rc = sqlite3_bind_int((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, signed int));
#endif
            break;

        case 'l':
#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
            rc = sqlite3_bind_int64((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, signed long long int));
#endif
            break;

        case 'f':
#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
            rc = sqlite3_bind_double((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, double));
#endif
            break;

        default:
            break;
        }

#if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
        if (rc != SQLITE_OK) {
            va_end(vl);
            return (0);
        }
#endif
    }

    va_end(vl);
    return (returnObject);
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

    /*~~~~~~~~~~~*/
    int     rc;
    void    *state;
    /*~~~~~~~~~~~*/

    state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                    "SELECT 1 FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1", domain,
                                    user);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return (rc == RUMBLE_DB_RESULT) ? 1 : 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_account_exists_raw(const char *user, const char *domain) {

    /*~~~~~~~~~~~*/
    int     rc;
    void    *state;
    /*~~~~~~~~~~~*/

    state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                    "SELECT 1 FROM accounts WHERE domain = %s AND user = %s ORDER BY LENGTH(user) DESC LIMIT 1", domain,
                                    user);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return (rc == RUMBLE_DB_RESULT) ? 1 : 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_mailbox *rumble_account_data(uint32_t uid, const char *user, const char *domain) {

    /*~~~~~~~~~~~~~~~~~~~*/
    int             rc;
    void            *state;
    char            *tmp;
    rumble_mailbox  *acc;
    /*~~~~~~~~~~~~~~~~~~~*/

    if (uid) {
        state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                        "SELECT id, domain, user, password, type, arg FROM accounts WHERE id = %u LIMIT 1", uid);
    } else {
        state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                        "SELECT id, domain, user, password, type, arg FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",
                                    domain ? domain : "", user ? user : "");
    }

    rc = rumble_database_run(state);
    acc = NULL;
    if (rc == RUMBLE_DB_RESULT) {

        /*~~*/
        int l;
        /*~~*/

        acc = (rumble_mailbox *) malloc(sizeof(rumble_mailbox));
        if (!acc) merror();

        /* Account UID */
        acc->uid = sqlite3_column_int((sqlite3_stmt *) state, 0);

        /* Account Domain struct */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
        acc->domain = rumble_domain_copy((const char *) sqlite3_column_text((sqlite3_stmt *) state, 1));

        /* Account Username */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 2);
        acc->user = (char *) calloc(1, l + 1);
        memcpy((char *) acc->user, sqlite3_column_text((sqlite3_stmt *) state, 2), l);

        /* Password (hashed) */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 3);
        acc->hash = (char *) calloc(1, l + 1);
        memcpy((char *) acc->hash, sqlite3_column_text((sqlite3_stmt *) state, 3), l);

        /* Account type */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 4);
        tmp = (char *) calloc(1, l + 1);
        if (!tmp) merror();
        memcpy((char *) tmp, sqlite3_column_text((sqlite3_stmt *) state, 4), l);
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
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 5);
        acc->arg = (char *) calloc(1, l + 1);
        memcpy((char *) acc->arg, sqlite3_column_text((sqlite3_stmt *) state, 5), l);
    }

    rumble_database_cleanup(state);
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

    /*~~~~~~~~~~~~~~~~~~~*/
    int             rc;
    void            *state;
    cvector         *cvec;
    rumble_mailbox  *acc;
    char            *tmp;
    /*~~~~~~~~~~~~~~~~~~~*/

    cvec = cvector_init();
    if (rumble_domain_exists(domain)) {
        state = rumble_database_prepare(rumble_database_master_handle->_core.db,
                                        "SELECT id, user, password, type, arg FROM accounts WHERE domain = %s", domain);
        acc = NULL;
        while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {

            /*~~*/
            int l;
            /*~~*/

            acc = (rumble_mailbox *) malloc(sizeof(rumble_mailbox));
            if (!acc) merror();

            /* Account UID */
            acc->uid = sqlite3_column_int((sqlite3_stmt *) state, 0);

            /* Account Username */
            l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
            acc->user = (char *) calloc(1, l + 1);
            memcpy((char *) acc->user, sqlite3_column_text((sqlite3_stmt *) state, 1), l);

            /* Password (hashed) */
            l = sqlite3_column_bytes((sqlite3_stmt *) state, 2);
            acc->hash = (char *) calloc(1, l + 1);
            memcpy((char *) acc->hash, sqlite3_column_text((sqlite3_stmt *) state, 2), l);

            /* Account type */
            l = sqlite3_column_bytes((sqlite3_stmt *) state, 3);
            tmp = (char *) calloc(1, l + 1);
            if (!tmp) merror();
            memcpy((char *) tmp, sqlite3_column_text((sqlite3_stmt *) state, 3), l);
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
            l = sqlite3_column_bytes((sqlite3_stmt *) state, 4);
            acc->arg = (char *) calloc(1, l + 1);
            memcpy((char *) acc->arg, sqlite3_column_text((sqlite3_stmt *) state, 4), l);
            acc->domain = 0;
            cvector_add(cvec, acc);
        }

        rumble_database_cleanup(state);
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
    int             rc,
                    l;
    void            *state;
    rumble_domain   *domain;
    d_iterator      iter;
    /*~~~~~~~~~~~~~~~~~~~~*/

    /* Clean up the old list */
    rumble_rw_start_write(rumble_database_master_handle->domains.rrw);
    foreach((rumble_domain *), domain, rumble_database_master_handle->domains.list, iter) {
        free(domain->name);
        free(domain->path);
        free(domain);
    }

    dvector_flush(rumble_database_master_handle->domains.list);
    state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, domain, storagepath FROM domains WHERE 1");
    while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {
        domain = (rumble_domain *) malloc(sizeof(rumble_domain));

        /* Domain ID */
        domain->id = sqlite3_column_int((sqlite3_stmt *) state, 0);

        /* Domain name */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
        domain->name = (char *) calloc(1, l + 1);
        memcpy(domain->name, sqlite3_column_text((sqlite3_stmt *) state, 1), l);

        /* Optional domain specific storage path */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 2);
        domain->path = (char *) calloc(1, l + 1);
        memcpy(domain->path, sqlite3_column_text((sqlite3_stmt *) state, 2), l);
        dvector_add(rumble_database_master_handle->domains.list, domain);
    }

    rumble_rw_stop_write(rumble_database_master_handle->domains.rrw);
    rumble_database_cleanup(state);
}
