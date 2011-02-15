/* 
 * File:   database.h
 * Author: Administrator
 *
 * Created on January 2, 2011, 5:59 PM
 */

#ifndef DATABASE_H
#define	DATABASE_H
#define RUMBLE_USING_SQLITE3
//#define RUMBLE_USING_MYSQL // uncomment to use MySQL instead of Sqlite

    #include "rumble.h"
    #include "sqlite3.h"

    sqlite3_stmt* rumble_sql_inject(sqlite3* db, const char* statement, ...);
    void* rumble_database_prepare(void* db, const char* statement, ...);
    void rumble_pop3_populate(sessionHandle* session, pop3Session* pops);

#ifdef RUMBLE_USING_SQLITE3
#define rumble_database_run(a) sqlite3_step((sqlite3_stmt*) a)
#define rumble_database_cleanup(a) sqlite3_finalize((sqlite3_stmt*) a)
#define RUMBLE_DB_RESULT SQLITE_ROW
#endif

void rumble_database_update_domains();

#endif	/* DATABASE_H */

