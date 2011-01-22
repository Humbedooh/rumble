#include "rumble.h"
#include "database.h"
#include <stdarg.h>



void rumble_database_load(masterHandle* master) {
    char* dbpath = (char*) calloc(1, strlen(rumble_config_str(master, "datafolder")) + 32);
    char* mailpath = (char*) calloc(1, strlen(rumble_config_str(master, "datafolder")) + 32);
	if(!dbpath||!mailpath) merror();
    sprintf(dbpath, "%s/rumble.sqlite", rumble_config_str(master, "datafolder"));
    sprintf(mailpath, "%s/mail.sqlite", rumble_config_str(master, "datafolder"));
    printf("Reading database...");
    
    // Domains and accounts
    if (sqlite3_open(dbpath, (sqlite3**) &master->_core.db)) { fprintf(stderr, "Can't open database <%s>: %s\n", dbpath, sqlite3_errmsg((sqlite3*) master->_core.db)); exit(EXIT_FAILURE); }
    
    // Letters
    if (sqlite3_open(mailpath, (sqlite3**) &master->_core.mail)) { fprintf(stderr, "Can't open database <%s>: %s\n", mailpath, sqlite3_errmsg((sqlite3*) master->_core.mail)); exit(EXIT_FAILURE); }
    
    free(dbpath);
    free(mailpath);
    printf("OK\n");    
}


userAccount* rumble_get_account(masterHandle* master, const char* user, const char* domain) {
    userAccount* ret = 0;
	char* tmp;
    const char* sql = "SELECT id,user,domain,type,arg FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1";
    sqlite3_stmt* state = (sqlite3_stmt*) rumble_database_prepare(master->_core.db,sql,domain,user);
    int rc = sqlite3_step(state);
    if ( rc == SQLITE_ROW ) {
        ssize_t l;
        ret = (userAccount*) calloc(1, sizeof(userAccount));
        if (!ret) merror();
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
		if(!tmp)merror();
        memcpy((char*) tmp, sqlite3_column_text(state,3), l);
        rumble_string_lower(tmp);
        ret->type = RUMBLE_MTYPE_MBOX;
        if (!strcmp(tmp, "alias")) ret->type = RUMBLE_MTYPE_ALIAS;
        else if (!strcmp(tmp, "mod")) ret->type = RUMBLE_MTYPE_MOD;
        else if (!strcmp(tmp, "feed")) ret->type = RUMBLE_MTYPE_FEED;
        free(tmp);
        // arg (if any)
        l = sqlite3_column_bytes(state,4);
        ret->arg = (char*) calloc(1,l+1);
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
	int rc;
    void* state;
	masterHandle* master = (masterHandle*) session->_master;
	state = rumble_database_prepare(master->_core.db,
		"SELECT * FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",\
		domain, user);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return ( rc == SQLITE_ROW) ? 1 : 0;
}

uint32_t rumble_domain_exists(sessionHandle* session, const char* domain) {
	masterHandle* master = (masterHandle*) session->_master;
	int rc;
    void* state;
	state = rumble_database_prepare(master->_core.db, "SELECT 1 FROM domains WHERE domain = %s LIMIT 1", domain);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return ( rc == SQLITE_ROW) ? 1 : 0;
}


// Wrapper for the SQL prepare statement
void* rumble_database_prepare(void* db, const char* statement, ...) {
	char *sql, b, *tmp;
	const char *p,*op;
	char injects[32];
	void* returnObject;
	va_list vl;
	int rc;
	ssize_t len = 0, strl = 0, at = 0;
	memset(injects, 0, 32);
	sql = (char*) calloc(1, 2048);
	if (!sql) merror();
	op = statement;
	for (p = strchr(statement, '%'); p != NULL; p = strchr(op, '%') ) {
		strl = strlen(op) - strlen(p);
		strncpy((char*) (sql + len), op, strl);
		len += strl;
		sscanf((const char*) p, "%%%c", &b);
		if ( b == '%' ) {
			strncpy((char*) (sql + len), "%", 1); len += 1;
		}
		else {
			strncpy((char*) (sql + len), "?", 1); len += 1;
			injects[at++] = b;
		}
		op = (char*) p + 2;
	}
	strl = strlen(op);
	strncpy((char*) (sql + len), op, strl);
#ifdef RUMBLE_USING_SQLITE3
	rc = sqlite3_prepare_v2((sqlite3*) db, sql, -1, (sqlite3_stmt**) &returnObject, NULL);
	free(sql);
	if ( rc != SQLITE_OK ) return 0;
#endif
	va_start(vl, statement);
	for ( at = 0; injects[at] != 0; at++) {
		switch (injects[at]) {
			case 's':
				#ifdef RUMBLE_USING_SQLITE3
				rc = sqlite3_bind_text((sqlite3_stmt*) returnObject, at+1, va_arg(vl, const char*), -1, SQLITE_TRANSIENT);
				#endif
				break;
			case 'u':
				#ifdef RUMBLE_USING_SQLITE3
				rc = sqlite3_bind_int((sqlite3_stmt*) returnObject, at+1, va_arg(vl, unsigned int));
				#endif
				break;
			case 'i':
				#ifdef RUMBLE_USING_SQLITE3
				rc = sqlite3_bind_int((sqlite3_stmt*) returnObject, at+1, va_arg(vl, signed int));
				#endif
				break;
			case 'f':
				#ifdef RUMBLE_USING_SQLITE3
				rc = sqlite3_bind_double((sqlite3_stmt*) returnObject, at+1, va_arg(vl, double));
				#endif
				break;
			default:
				break;
		}
		#ifdef RUMBLE_USING_SQLITE3
		if ( rc != SQLITE_OK ) {
			va_end(vl);
			return 0;
		}
		#endif
	}
	va_end(vl);
//	rc = sqlite3_step((sqlite3_stmt*) returnObject);
	
	return returnObject;
}
