#include "rumble.h"
#include "database.h"
#include <stdarg.h>



void rumble_database_load(masterHandle* master) {
    char* dbpath = calloc(1, strlen(rumble_config_str("datafolder")) + 32);
    char* mailpath = calloc(1, strlen(rumble_config_str("datafolder")) + 32);
    sprintf(dbpath, "%s/rumble.sqlite", rumble_config_str("datafolder"));
    sprintf(mailpath, "%s/mail.sqlite", rumble_config_str("datafolder"));
    printf("Reading database...");
    
    // Domains and accounts
    if (sqlite3_open(dbpath, (sqlite3**) &master->readOnly.db)) { fprintf(stderr, "Can't open database <%s>: %s\n", dbpath, sqlite3_errmsg((sqlite3*) master->readOnly.db)); exit(EXIT_FAILURE); }
    
    // Letters
    if (sqlite3_open(mailpath, (sqlite3**) &master->readOnly.mail)) { fprintf(stderr, "Can't open database <%s>: %s\n", mailpath, sqlite3_errmsg((sqlite3*) master->readOnly.mail)); exit(EXIT_FAILURE); }
    
    free(dbpath);
    free(mailpath);
    printf("OK\n");    
}


uint32_t rumble_account_exists(sessionHandle* session, const char* user, const char* domain) {
    const char* sql = "SELECT * FROM accounts WHERE `domain` = \"%s\" AND \"%s\" GLOB `user` ORDER BY LENGTH(`user`) DESC LIMIT 1";
    char* clause = calloc(1, strlen(sql) + 256);
    sprintf(clause, sql, domain, user);
     masterHandle* master = (masterHandle*) session->_master;
    int rc;
    sqlite3_stmt* state;
    sqlite3_prepare_v2((sqlite3*) master->readOnly.db, clause, -1, &state, NULL);
    rc = sqlite3_step(state);
    sqlite3_finalize(state);
    free(clause);
    return ( rc == SQLITE_ROW) ? 1 : 0;
}

uint32_t rumble_domain_exists(sessionHandle* session, const char* domain) {
    const char* sql = "SELECT * FROM domains WHERE `domain` = \"%s\" LIMIT 1";
    char* clause = calloc(1, strlen(sql) + 128);
    sprintf(clause, sql, domain);
     masterHandle* master = (masterHandle*) session->_master;
    int rc;
    sqlite3_stmt* state;
    sqlite3_prepare_v2((sqlite3*) master->readOnly.db, clause, -1, &state, NULL);
    rc = sqlite3_step(state);
    sqlite3_finalize(state);
    free(clause);
    return ( rc == SQLITE_ROW) ? 1 : 0;
}

sqlite3_stmt* rumble_sql_inject(sqlite3* db, const char* statement, ...) {
    ssize_t count = 0;
    ssize_t len = strlen(statement);
    ssize_t x;
    for ( x = 0; x < len; x++ ) { if (statement[x] == '?') count++; }
    sqlite3_stmt* state;
    sqlite3_prepare_v2(db, statement, -1, &state, NULL);
    va_list vl;
    va_start(vl,statement);
    const char* val;
    for (x = 0; x < count; x++) {
        val = va_arg(vl, const char*);
        sqlite3_bind_text(state, x+1, val ? val : "", -1, SQLITE_TRANSIENT);
    }
    return state;
}