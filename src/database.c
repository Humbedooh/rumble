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


void rumble_free_account(userAccount* user) {
    if ( user->arg ) free(user->arg);
    if ( user->domain) free(user->domain);
    if ( user->user) free(user->user);
	if ( user->hash) free(user->hash);
    user->arg = 0;
    user->domain = 0;
    user->user = 0;
}

uint32_t rumble_account_exists(sessionHandle* session, const char* user, const char* domain) {
	int rc;
    void* state;
	masterHandle* master = (masterHandle*) session->_master;
	state = rumble_database_prepare(master->_core.db,
		"SELECT 1 FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",\
		domain, user);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return ( rc == RUMBLE_DB_RESULT) ? 1 : 0;
}

userAccount* rumble_account_data(sessionHandle* session, const char* user, const char* domain) {
	int rc;
    void* state;
	char* tmp;
	userAccount* acc;
	masterHandle* master = (masterHandle*) session->_master;
	state = rumble_database_prepare(master->_core.db,
		"SELECT id, domain, user, password, type, arg FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",\
		domain, user);
    rc = rumble_database_run(state);
    acc = NULL;
	if ( rc == RUMBLE_DB_RESULT ) { 
		int l;
		acc = (userAccount*) malloc(sizeof(userAccount));
		if (!acc) merror();
		printf("got a result, fetching it...\n");
		// Account UID
		acc->uid = sqlite3_column_int((sqlite3_stmt*) state, 0);

		// Account Domain
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		acc->domain = (char*) calloc(1,l+1);
		memcpy((char*) acc->domain, sqlite3_column_text((sqlite3_stmt*) state,1), l);

		// Account Username
		l = sqlite3_column_bytes((sqlite3_stmt*) state,2);
		acc->user = (char*) calloc(1,l+1);
		memcpy((char*) acc->user, sqlite3_column_text((sqlite3_stmt*) state,2), l);

		// Password (hashed)
		l = sqlite3_column_bytes((sqlite3_stmt*) state,3);
		acc->hash = (char*) calloc(1,l+1);
		memcpy((char*) acc->hash, sqlite3_column_text((sqlite3_stmt*) state,3), l);

		// Account type
		l = sqlite3_column_bytes((sqlite3_stmt*) state,4);
        tmp = (char*) calloc(1,l+1);
		if (!tmp) merror();
        memcpy((char*) tmp, sqlite3_column_text((sqlite3_stmt*) state,4), l);
        rumble_string_lower(tmp);
        acc->type = RUMBLE_MTYPE_MBOX;
        if (!strcmp(tmp, "alias")) acc->type = RUMBLE_MTYPE_ALIAS;
        else if (!strcmp(tmp, "mod")) acc->type = RUMBLE_MTYPE_MOD;
        else if (!strcmp(tmp, "feed")) acc->type = RUMBLE_MTYPE_FEED;
        free(tmp);

		// Account args
		l = sqlite3_column_bytes((sqlite3_stmt*) state,5);
		acc->arg = (char*) calloc(1,l);
		memcpy((char*) acc->arg, sqlite3_column_text((sqlite3_stmt*) state,5), l);

	}
	else printf("no such user or something: %u\n", rc);
	rumble_database_cleanup(state);

    return acc;
}

void rumble_pop3_populate(sessionHandle* session, pop3Session* pops) {
	int rc,l;
    void* state;
	char* tmp;
	pop3Letter* letter;
	masterHandle* master = (masterHandle*) session->_master;
	state = rumble_database_prepare(master->_core.db, "SELECT id, fid, size FROM mbox WHERE uid = %u", pops->account->uid);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		letter = (pop3Letter*) malloc(sizeof(pop3Letter));

		// Letter ID
		letter->id = sqlite3_column_int((sqlite3_stmt*) state, 0);

		// Letter File ID
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		letter->fid = (char*) calloc(1,l+1);
		memcpy((char*) letter->fid, sqlite3_column_text((sqlite3_stmt*) state,1), l);

		// Letter Size
		letter->size = sqlite3_column_int((sqlite3_stmt*) state, 2);

		cvector_add(pops->letters, letter);
	}
	rumble_database_cleanup(state);
}

uint32_t rumble_domain_exists(sessionHandle* session, const char* domain) {
	masterHandle* master = (masterHandle*) session->_master;
	int rc;
    void* state;
	state = rumble_database_prepare(master->_core.db, "SELECT 1 FROM domains WHERE domain = %s LIMIT 1", domain);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return ( rc == RUMBLE_DB_RESULT) ? 1 : 0;
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
	return returnObject;
}
