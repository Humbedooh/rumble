#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
masterHandle* rumble_database_master_handle = 0;

/* Database constructors and wrappers */

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
			case 'l':
				#ifdef RUMBLE_USING_SQLITE3
				rc = sqlite3_bind_int64((sqlite3_stmt*) returnObject, at+1, va_arg(vl, signed int));
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


void rumble_free_account(rumble_mailbox* user) {
    if ( user->arg ) free(user->arg);
    if ( user->user) free(user->user);
	if ( user->hash) free(user->hash);
	if ( user->domain ) {
		if (user->domain->name) free(user->domain->name);
		if (user->domain->path) free(user->domain->path);
		free(user->domain);
	}
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

rumble_mailbox* rumble_account_data(sessionHandle* session, const char* user, const char* domain) {
	int rc;
    void* state;
	char* tmp;
	rumble_mailbox* acc;
	masterHandle* master = (masterHandle*) session->_master;
	state = rumble_database_prepare(master->_core.db,
		"SELECT id, domain, user, password, type, arg FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",\
		domain, user);
    rc = rumble_database_run(state);
    acc = NULL;
	if ( rc == RUMBLE_DB_RESULT ) { 
		int l;
		acc = (rumble_mailbox*) malloc(sizeof(rumble_mailbox));
		if (!acc) merror();
		
		// Account UID
		acc->uid = sqlite3_column_int((sqlite3_stmt*) state, 0);

		// Account Domain struct
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		acc->domain = rumble_domain_copy((const char*) sqlite3_column_text((sqlite3_stmt*) state,1));

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
		else if (!strcmp(tmp, "relay")) acc->type = RUMBLE_MTYPE_FEED;
        free(tmp);

		// Account args
		l = sqlite3_column_bytes((sqlite3_stmt*) state,5);
		acc->arg = (char*) calloc(1,l+1);
		memcpy((char*) acc->arg, sqlite3_column_text((sqlite3_stmt*) state,5), l);

	}
	rumble_database_cleanup(state);

    return acc;
}

rumble_mailbox* rumble_account_data_auth(sessionHandle* session, const char* user, const char* domain, const char* pass) {
	rumble_mailbox* acc;
	char *hash;
	masterHandle* master = (masterHandle*) session->_master;
	acc = rumble_account_data(session, user, domain);
	if ( acc ) {
		hash = rumble_sha256((const unsigned char*) pass);
		if (!strcmp(hash, acc->hash)) return acc;
		rumble_free_account(acc);
		acc = 0;
	}
    return acc;
}

/* rumble_domain_exists: Checks if a domain exists in the database. Returns 1 if true, 0 if false. */
uint32_t rumble_domain_exists(const char* domain) {
	uint32_t rc;
	rumble_domain* dmn;
	rc = 0;
	rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
	for ( dmn = (rumble_domain*) cvector_first(rumble_database_master_handle->domains.list); dmn != NULL; dmn = (rumble_domain*) cvector_next(rumble_database_master_handle->domains.list) ) {
		if ( !strcmp(dmn->name, domain) ) { rc = 1; break; }
	}
	rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
	return rc;
}

/* rumble_domain_copy: Returns a copy of the domain info */
rumble_domain* rumble_domain_copy(const char* domain) {
	rumble_domain *dmn, *rc;
	rc = (rumble_domain*) malloc(sizeof(rumble_domain));
	rc->id = 0;
	rc->path = 0;
	rc->name = 0;

	rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
	for ( dmn = (rumble_domain*) cvector_first(rumble_database_master_handle->domains.list); dmn != NULL; dmn = (rumble_domain*) cvector_next(rumble_database_master_handle->domains.list) ) {
		if ( !strcmp(dmn->name, domain) ) { 
			rc->name = (char*) calloc(1, strlen(dmn->name)+1);
			rc->path = (char*) calloc(1, strlen(dmn->path)+1);
			strcpy(rc->name, dmn->name);
			strcpy(rc->path, dmn->path);
			rc->id = dmn->id;
			break;
		}
	}
	rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
	return rc;
}



/* LETTER HANDLING FUNCTIONS FOR SMTP, POP3 & IMAP4 */

/* rumble_letters_retrieve: retrieves all letters for an account and stores them in a mail bag struct */


rumble_mailbag* rumble_letters_retrieve_folder(rumble_mailbox* acc, ssize_t folder) {
	int rc,l;
    void* state;
	rumble_mailbag* bag;
	rumble_letter* letter;
	rumbleIntValuePair* pair;
	bag = (rumble_mailbag*) malloc(sizeof(rumble_mailbag));
	bag->size = 0;
	bag->account = acc;
	bag->contents = cvector_init();
	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, fid, size, delivered, flags FROM mbox WHERE folder = %u AND uid = %u", folder, acc->uid);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		letter = (rumble_letter*) malloc(sizeof(rumble_letter));

		// Letter ID
		letter->id = sqlite3_column_int((sqlite3_stmt*) state, 0);

		// Letter File ID
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		letter->fid = (char*) calloc(1,l+1);
		memcpy((char*) letter->fid, sqlite3_column_text((sqlite3_stmt*) state,1), l);

		// Letter Size
		letter->size = sqlite3_column_int((sqlite3_stmt*) state, 2);

		// Delivery date
		letter->delivered = sqlite3_column_int((sqlite3_stmt*) state, 3);

		// Flags
		letter->flags = sqlite3_column_int((sqlite3_stmt*) state, 4);
		letter->_flags = letter->flags;

		// UID
		letter->uid = acc->uid;
		
		cvector_add(bag->contents, letter);
		bag->size++;
	}
	rumble_database_cleanup(state);
	bag->letters = (rumble_letter**) malloc(sizeof(rumble_letter)*bag->size);
	l = 0;
	for ( letter = (rumble_letter*) cvector_first(bag->contents); letter != NULL; letter = (rumble_letter*) cvector_next(bag->contents)) {
		bag->letters[l++] = letter;
	}

	return bag;
}


/* rumble_letters_retrieve: retrieves all letters for an account and stores them in a mail bag struct */




/* rumble_letters_expunge: purges the mailbag, removing letters marked for deletion */
uint32_t rumble_letters_expunge(rumble_mailbag* bag) {
	int r,x,y;
	void* state;
	const char* path;
	rumble_letter* letter, **letters;
	char tmp[256];
	if (!bag) return 0;
	path = strlen(bag->account->domain->path) ? bag->account->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
	r = 0;
	y = bag->size;
	for ( x = 0; x < y; x++ ) {
		letter = bag->letters[x];
		if (letter->flags & RUMBLE_LETTER_EXPUNGE) {
			bag->letters[x] = 0;
			bag->size--;
		}
	}
	if ( bag->size != y ) {
		letters = (rumble_letter**) malloc(sizeof(rumble_letter*)*bag->size);
		bag->size = 0;
		for ( x = 0; x < y; x++ ) {
			if ( bag->letters[x] ) letters[bag->size++] = bag->letters[x];
		}
		free(bag->letters);
		bag->letters = letters;
	}
	r = 0;
	for ( letter = (rumble_letter*) cvector_first(bag->contents); letter != NULL; letter = (rumble_letter*) cvector_next(bag->contents)) {
		if ( (letter->flags & RUMBLE_LETTER_EXPUNGE)  ) { /* Delete it? */
			sprintf(tmp, "%s/%s.msg", path, letter->fid);
			unlink(tmp);
			state = rumble_database_prepare(rumble_database_master_handle->_core.db, "DELETE FROM mbox WHERE id = %u", letter->id);
			rumble_database_run(state);
			rumble_database_cleanup(state);
			r++;
			free(letter->fid);
			free(letter);
			cvector_delete(bag->contents);
		}
	}
	return r;
}

/* rumble_letters_update: Updates letters in the database if changed */
uint32_t rumble_letters_update(rumble_mailbag* bag) {
	int r;
	void* state;
	rumble_letter* letter;
	if (!bag) return 0;
	r = 0;
	for ( letter = (rumble_letter*) cvector_first(bag->contents); letter != NULL; letter = (rumble_letter*) cvector_next(bag->contents)) {
		if ( letter->flags != letter->_flags ) {
			printf("Updating letter no. %u (%08x -> %08x)\r\n", letter->id, letter->_flags, letter->flags);
			if (letter->flags & RUMBLE_LETTER_UPDATED) letter->flags -= RUMBLE_LETTER_UPDATED;
			state = rumble_database_prepare(rumble_database_master_handle->_core.db, "UPDATE mbox SET flags = %u WHERE id = %u", letter->flags, letter->id);
			rumble_database_run(state);
			rumble_database_cleanup(state);
			r++;
		}
	}
	return r;
}

/* rumble_letters_flush: Flushes a mail bag, freeing up memory. */
void rumble_letters_flush(rumble_mailbag* bag) {
	rumble_letter* letter;
	rumbleIntValuePair* pair;
	if (!bag || !bag->contents) return;
	for ( letter = (rumble_letter*) cvector_first(bag->contents); letter != NULL; letter = (rumble_letter*) cvector_next(bag->contents)) {
		free(letter->fid);
		free(letter);
	}
	cvector_flush(bag->contents);
	free(bag->letters);
	bag->account = 0;
	bag->letters = 0;
	free(bag);
}

cvector* rumble_folders_retrieve(rumble_mailbox* acc) {
	cvector* folders;
	rumble_folder* pair;
	int rc,l;
	void* state;
	folders = cvector_init();
	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", acc->uid);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		pair = (rumble_folder*) malloc(sizeof(rumble_folder));

		// Folder ID
		pair->id = sqlite3_column_int((sqlite3_stmt*) state, 0);

		// Folder name
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		pair->name = (char*) calloc(1,l+1);
		memcpy((char*) pair->name, sqlite3_column_text((sqlite3_stmt*) state,1), l);

		// Subscribed?
		pair->subscribed = sqlite3_column_int((sqlite3_stmt*) state, 2);

		cvector_add(folders, pair);
	}
	rumble_database_cleanup(state);
	return folders;
}

void rumble_folders_flush(cvector* folders) {
	rumble_folder* pair;
	if (folders) {
		for ( pair = (rumble_folder*) cvector_first(folders); pair != NULL; pair = (rumble_folder*) cvector_next(folders)) {
			free(pair->name);
			free(pair);
		}
		cvector_flush(folders);
		free(folders);
	}
}

/* rumble_letters_open: Opens a letter as a file handle */
FILE* rumble_letters_open(rumble_mailbox* mbox, rumble_letter* letter) {
	char file[512];
	sprintf(file, "%s/%s.msg", strlen(mbox->domain->path) ? mbox->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder"), letter->fid);
	return fopen(file, "rb");
}


/* Internal database functions (not for use by modules) */

/* rumble_database_update_domains: Updates the list of domains from the db */
void rumble_database_update_domains() {
	int rc,l;
    void* state;
	rumble_domain* domain;

	/* Clean up the old list */
	rumble_rw_start_write(rumble_database_master_handle->domains.rrw);
	for ( domain = (rumble_domain*) cvector_first(rumble_database_master_handle->domains.list); domain != NULL; domain = (rumble_domain*) cvector_next(rumble_database_master_handle->domains.list) ) {
		free(domain->name);
		free(domain->path);
		free(domain);
	}
	cvector_flush(rumble_database_master_handle->domains.list);

	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, domain, storagepath FROM domains WHERE 1");
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		domain = (rumble_domain*) malloc(sizeof(rumble_domain));

		/* Domain ID */
		domain->id = sqlite3_column_int((sqlite3_stmt*) state, 0);
		
		/* Domain name */
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		domain->name = (char*) calloc(1,l+1);
		memcpy(domain->name, sqlite3_column_text((sqlite3_stmt*) state,1), l);

		/* Optional domain specific storage path */
		l = sqlite3_column_bytes((sqlite3_stmt*) state,2);
		domain->path = (char*) calloc(1,l+1);
		memcpy(domain->path, sqlite3_column_text((sqlite3_stmt*) state,2), l);

		cvector_add(rumble_database_master_handle->domains.list, domain);
	}
	rumble_rw_stop_write(rumble_database_master_handle->domains.rrw);
	rumble_database_cleanup(state);
}





/* NEW IMAP4 STUFF GOES HERE */

/* rumble_letters_retrieve_shared(acc):
 * Retrieves a completed, shared instance of a mailbag for IMAP4 use.
 * The bag can be shared across multiple connections for faster processing.
*/
rumble_imap4_shared_bag* rumble_letters_retrieve_shared(rumble_mailbox* acc) {
	int rc,l;
    void* state;
	rumble_imap4_shared_bag* bag;
	rumble_letter* letter;
	rumble_imap4_shared_folder* folder;
	bag = (rumble_imap4_shared_bag*) malloc(sizeof(rumble_imap4_shared_bag));
	bag->folders = cvector_init();
	bag->rrw = rumble_rw_init();

	/* Add the default inbox */
	folder = (rumble_imap4_shared_folder*) malloc(sizeof(rumble_imap4_shared_folder));
	folder->id = 0;
	folder->letters = cvector_init();
	folder->lastMessage = 0;
	folder->updated = time(0);
	folder->subscribed = 1;
	folder->name = (char*) calloc(1,32); strcpy(folder->name, "INBOX");
	cvector_add(bag->folders, folder);

	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", acc->uid);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		folder = (rumble_imap4_shared_folder*) malloc(sizeof(rumble_imap4_shared_folder));

		// Folder ID
		folder->id = sqlite3_column_int64((sqlite3_stmt*) state, 0);

		// Folder name
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		folder->name = (char*) calloc(1,l+1);
		strncpy(folder->name, (char*) sqlite3_column_text((sqlite3_stmt*) state,1), l);

		// Subscribed?
		folder->subscribed = sqlite3_column_int((sqlite3_stmt*) state, 2);

		folder->letters = cvector_init();
		folder->updated = time(0);
		folder->lastMessage = 0;
		cvector_add(bag->folders, folder);
				printf("Added folder: %s (%u)\n", folder->name, folder->id);
	}
	rumble_database_cleanup(state);


	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE uid = %u", acc->uid);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		letter = (rumble_letter*) malloc(sizeof(rumble_letter));

		// Letter ID
		letter->id = sqlite3_column_int64((sqlite3_stmt*) state, 0);

		// Letter File ID
		l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
		letter->fid = (char*) calloc(1,l+1);
		memcpy((char*) letter->fid, sqlite3_column_text((sqlite3_stmt*) state,1), l);

		// Letter Size
		letter->size = sqlite3_column_int((sqlite3_stmt*) state, 2);

		// Delivery date
		letter->delivered = sqlite3_column_int((sqlite3_stmt*) state, 3);

		// Flags
		letter->flags = sqlite3_column_int((sqlite3_stmt*) state, 4);
		letter->_flags = letter->flags;

		// UID
		letter->uid = acc->uid;

		letter->folder = sqlite3_column_int64((sqlite3_stmt*) state, 5);
		l = 0;
		for ( folder = (rumble_imap4_shared_folder*) cvector_first(bag->folders); folder != NULL; folder = (rumble_imap4_shared_folder*) cvector_next(bag->folders) ) {
			if ( folder->id == letter->folder ) { 
				l++;
				cvector_add(folder->letters, letter);
				folder->lastMessage = (folder->lastMessage < letter->id) ? letter->id : folder->lastMessage;
				break;
			}
		}
		if (!l) {
			free(letter->fid);
			free(letter);
		}
	}
	rumble_database_cleanup(state);

	return bag;
}

rumble_imap4_shared_folder* rumble_imap4_current_folder(imap4Session* sess) {
	rumble_imap4_shared_folder* folder;
	for ( folder = (rumble_imap4_shared_folder*) cvector_first(sess->bag->folders); folder != NULL; folder = (rumble_imap4_shared_folder*) cvector_next(sess->bag->folders) ) {
		if ( folder->id == sess->folder ) return folder;
	}
	printf("<curfolder> Couldn't find folder no. %lld(?)\n", sess->folder);
	return 0;
}


/* rumble_imap4_update_folders: Updates the list of folders in the mail account */
void rumble_imap4_update_folders(rumble_imap4_shared_bag* bag) {
	rumble_imap4_shared_folder* folder;
	int rc,l, folder_id, found;
	void* state;

	rumble_rw_start_write(bag->rrw); // Lock bag for writing
	
	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, name, subscribed FROM folders WHERE uid = %u", bag->uid);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {

		// Get the folder ID
		folder_id = sqlite3_column_int((sqlite3_stmt*) state, 0);

		// Match against our existing folders and add if not there.
		found = 0;
		for ( folder = (rumble_imap4_shared_folder*) cvector_first(bag->folders); folder != NULL; folder = (rumble_imap4_shared_folder*) cvector_next(bag->folders)) {
			if ( folder->id == folder_id ) { found++; break; }
		}

		if (!found) {
			folder = (rumble_imap4_shared_folder*) malloc(sizeof(rumble_imap4_shared_folder));
			folder->id = folder_id;

			// Folder name
			l = sqlite3_column_bytes((sqlite3_stmt*) state,1);
			folder->name = (char*) calloc(1,l+1);
			memcpy((char*) folder->name, sqlite3_column_text((sqlite3_stmt*) state,1), l);

			// Subscribed?
			folder->subscribed = sqlite3_column_int((sqlite3_stmt*) state, 2);
			cvector_add(bag->folders, folder);
		}
	}
	rumble_database_cleanup(state);


	rumble_rw_stop_write(bag->rrw); // Unlock bag again.
}



uint32_t rumble_imap4_scan_incoming(rumble_imap4_shared_folder* folder) {
	int r,rc,exists;
	void* state;
	rumble_letter* letter;
	if (!folder) return 0;
	r = 0;
	state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, fid, size, delivered, flags, folder FROM mbox WHERE folder = %l AND id > %u", folder->id, folder->lastMessage);
    while ( (rc = rumble_database_run(state)) == RUMBLE_DB_RESULT ) {
		r++;
		exists = 0;
		
	}
	rumble_database_run(state);
	rumble_database_cleanup(state);

	return r;
}


/* rumble_imap4_commit: Commits any changes done to the folder, deleting deleted letters and updating any flags set */
uint32_t rumble_imap4_commit(imap4Session* imap, rumble_imap4_shared_folder* folder) {
	int r,x,y;
	void* state;
	const char* path;
	rumble_letter* letter, **letters;
	char tmp[256];
	if (!folder) return 0;
	path = strlen(imap->account->domain->path) ? imap->account->domain->path : rrdict(rumble_database_master_handle->_core.conf, "storagefolder");
	r = 0;

	rumble_rw_start_write(imap->bag->rrw); // Lock the bag

	for ( letter = (rumble_letter*) cvector_first(folder->letters); letter != NULL; letter = (rumble_letter*) cvector_next(folder->letters)) {
		if ( (letter->flags & RUMBLE_LETTER_EXPUNGE)  ) { /* Delete it? */
			sprintf(tmp, "%s/%s.msg", path, letter->fid);
			unlink(tmp);
			state = rumble_database_prepare(rumble_database_master_handle->_core.db, "DELETE FROM mbox WHERE id = %l", letter->id);
			rumble_database_run(state);
			rumble_database_cleanup(state);
			r++;
			free(letter->fid);
			free(letter);
			cvector_delete(folder->letters);
		}
		else if ( letter->flags != letter->_flags ) {
			printf("Updating letter no. %llu (%08x -> %08x)\r\n", letter->id, letter->_flags, letter->flags);
			if (letter->flags & RUMBLE_LETTER_UPDATED) letter->flags -= RUMBLE_LETTER_UPDATED;
			state = rumble_database_prepare(rumble_database_master_handle->_core.db, "UPDATE mbox SET flags = %u WHERE id = %l", letter->flags, letter->id);
			rumble_database_run(state);
			rumble_database_cleanup(state);
			r++;
		}
	}

	rumble_rw_stop_write(imap->bag->rrw); // Unlock the bag
	return r;
}