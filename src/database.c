#include "rumble.h"
#include "database.h"
#include <stdarg.h>


/* char* rumble_sha256(const unsigned char* d)
 * Converts the string (d) into a SHA-256 digest (64 byte hex string).
 * Note: For extra speed, digests are printed out "backwards" as:
 * DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA
 * This should have no effect on security and is roughly 3 times faster.
*/
char* rumble_sha256(const unsigned char* d) {
    unsigned char* md = malloc(33);
    char* ret = malloc(65);
    SHA256(d, strlen((const char*) d), md);
    unsigned int* x = (unsigned int*) md;
    sprintf((char*) ret, "%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32, x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7]);
    memset(md, 0, 33); // Erase md, just in case.
    free(md);
    return ret;
}

/* char* rumble_sha160(const unsigned char* d)
 * Converts the string (d) into a hex SHA1 160 bit digest (40 byte hex string).
 * This is used for simpler tasks, such as grey-listing, where collisions are
 * of less importance.
 * Note: For extra speed, digests are printed out "backwards" as:
 * DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA
 * This should have no effect on security and is roughly 3 times faster.
*/
char* rumble_sha160(const unsigned char* d) {
    unsigned char* md = malloc(21);
    char* ret = malloc(41);
    SHA1(d, strlen((const char*) d), md);
    unsigned int* x = (unsigned int*) md;
    sprintf((char*) ret, "%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32"%08"PRIx32, x[0],x[1],x[2],x[3],x[4]);
    memset(md, 0, 21); // Erase md, just in case.
    free(md);
    return ret;
}


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
        sqlite3_bind_text(state, x+1, val, -1, SQLITE_TRANSIENT);
    }
    return state;
}