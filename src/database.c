#include "rumble.h"
#include "database.h"
#include <stdarg.h>



void rumble_database_load(masterHandle* master) {
    char* dbpath = (char*) calloc(1, strlen(rumble_config_str(master, "datafolder")) + 32);
    char* mailpath = (char*) calloc(1, strlen(rumble_config_str(master, "datafolder")) + 32);
    sprintf(dbpath, "%s/rumble.sqlite", rumble_config_str(master, "datafolder"));
    sprintf(mailpath, "%s/mail.sqlite", rumble_config_str(master, "datafolder"));
    printf("Reading database...");
    
    // Domains and accounts
    if (sqlite3_open(dbpath, (sqlite3**) &master->readOnly.db)) { fprintf(stderr, "Can't open database <%s>: %s\n", dbpath, sqlite3_errmsg((sqlite3*) master->readOnly.db)); exit(EXIT_FAILURE); }
    
    // Letters
    if (sqlite3_open(mailpath, (sqlite3**) &master->readOnly.mail)) { fprintf(stderr, "Can't open database <%s>: %s\n", mailpath, sqlite3_errmsg((sqlite3*) master->readOnly.mail)); exit(EXIT_FAILURE); }
    
    free(dbpath);
    free(mailpath);
    printf("OK\n");    
}


userAccount* rumble_get_account(masterHandle* master, const char* user, const char* domain) {
    userAccount* ret = 0;
	char* tmp;
    const char* sql = "SELECT id,user,domain,type,arg FROM accounts WHERE domain = ? AND ? GLOB user ORDER BY LENGTH(user) DESC LIMIT 1";
    sqlite3_stmt* state = rumble_sql_inject((sqlite3*) master->readOnly.db,sql,domain,user);
    int rc = sqlite3_step(state);
    if ( rc == SQLITE_ROW ) {
        ssize_t l;
        ret = (userAccount*) calloc(1, sizeof(userAccount));
        
        // user ID
        ret->uid = sqlite3_column_int(state, 0);
        
        // user
        l = sqlite3_column_bytes(state,1);
        ret->user = (char*) calloc(1,l+1);
        memcpy((char*) ret->user, sqlite3_column_text(state,1), l);
        
        // domain
        l = sqlite3_column_bytes(state,2);
        ret->domain = (char*) calloc(1,l+1);
        memcpy((char*) ret->domain, sqlite3_column_text(state,2), l);
        
        // mbox type (alias, mbox, prog)
        l = sqlite3_column_bytes(state,3);
        tmp = (char*) calloc(1,l+1);
        memcpy((char*) tmp, sqlite3_column_text(state,3), l);
        rumble_string_lower(tmp);
        ret->type = RUMBLE_MTYPE_MBOX;
        if (!strcmp(tmp, "alias")) ret->type = RUMBLE_MTYPE_ALIAS;
        else if (!strcmp(tmp, "mod")) ret->type = RUMBLE_MTYPE_MOD;
        else if (!strcmp(tmp, "feed")) ret->type = RUMBLE_MTYPE_FEED;
        free(tmp);
        // arg (if any)
        l = sqlite3_column_bytes(state,4);
        ret->arg = calloc(1,l+1);
        memcpy((char*) ret->arg, sqlite3_column_text(state,4), l);
    }
    sqlite3_finalize(state);
    return ret;
}

void rumble_free_account(userAccount* user) {
    if ( user->arg ) free(user->arg);
    if ( user->domain) free(user->domain);
    if ( user->user) free(user->user);
    user->arg = 0;
    user->domain = 0;
    user->user = 0;
}

uint32_t rumble_account_exists(sessionHandle* session, const char* user, const char* domain) {
    const char* sql = "SELECT * FROM accounts WHERE `domain` = \"%s\" AND \"%s\" GLOB `user` ORDER BY LENGTH(`user`) DESC LIMIT 1";
    char* clause = (char*) calloc(1, strlen(sql) + 256);
    masterHandle* master = (masterHandle*) session->_master;
    int rc;
    sqlite3_stmt* state;
	sprintf(clause, sql, domain, user);
    sqlite3_prepare_v2((sqlite3*) master->readOnly.db, clause, -1, &state, NULL);
    rc = sqlite3_step(state);
    sqlite3_finalize(state);
    free(clause);
    return ( rc == SQLITE_ROW) ? 1 : 0;
}

uint32_t rumble_domain_exists(sessionHandle* session, const char* domain) {
    const char* sql = "SELECT * FROM domains WHERE `domain` = \"%s\" LIMIT 1";
    char* clause = (char*) calloc(1, strlen(sql) + 128);
	masterHandle* master = (masterHandle*) session->_master;
	int rc;
    sqlite3_stmt* state;
	sprintf(clause, sql, domain);
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
	sqlite3_stmt* state;
	va_list vl;
	const char* val;
    for ( x = 0; x < len; x++ ) { if (statement[x] == '?') count++; }
    sqlite3_prepare_v2(db, statement, -1, &state, NULL);
    va_start(vl,statement);
    for (x = 0; x < count; x++) {
        val = va_arg(vl, const char*);
        sqlite3_bind_text(state, x+1, val ? val : "", -1, SQLITE_TRANSIENT);
    }
    return state;
}