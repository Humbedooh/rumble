/* File: database.h Author: Administrator Created on January 2, 2011, 5:59 PM */
#ifndef DATABASE_H
#   define DATABASE_H

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Supported database models
 -----------------------------------------------------------------------------------------------------------------------
 */

#   define RUMBLE_SQLITE3          1
#   define RUMBLE_MYSQL            2
#   define RUMBLE_POSTGRESQL       3
#   define RUMBLE_DATABASE_MODEL   RUMBLE_SQLITE3
typedef struct
{
    void        *state;
    void        *db;
    unsigned    result;
    char        buffer[1024];
} dbObject;
typedef struct
{
    enum { STRING = 1, NUMBER = 2 }           type;
    unsigned    size;
    void        *data;
} dbItem;
typedef struct
{
    dbItem      *column;
    unsigned    items;
} dbResult;
typedef struct
{
    unsigned    inUse;
    void        *handle;
} dbChild;
typedef struct
{
    dbChild *children;

    /*
     * rumble_readerwriter rrw;
     */
} dbPool;

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Includes
 -----------------------------------------------------------------------------------------------------------------------
 */

#   include "rumble.h"

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Fixed prototypes
 -----------------------------------------------------------------------------------------------------------------------
 */

int         rumble_database_do(void *db, const char *statement, ...);
dbObject    *rumble_database_prepare(void *db, const char *statement, ...);
void        rumble_database_free_result(dbResult *result);
#   define rdbfree(a)  rumble_database_free_result(a)
dbResult    *rumble_database_fetch_row(void *state);
dbResult    *rumble_database_step(dbObject *dbo);
void        rumble_database_cleanup(dbObject *dbo);
#   define rdbas(a, b) *((a *) b)
#   include <mysql.h>

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Model-specific definitions
 -----------------------------------------------------------------------------------------------------------------------
 */

void        rumble_database_load_sqlite(masterHandle *master, FILE *runlog);
void        rumble_database_prepare_sqlite(dbObject *dbo, const char *statement, va_list vl);
dbResult    *rumble_database_fetch_row_sqlite(void *state);
#   ifdef MYSQL_CLIENT
void        rumble_database_load_mysql(masterHandle *master, FILE *runlog);
void        rumble_database_prepare_mysql(dbObject *dbo, const char *statement, va_list vl);
dbResult    *rumble_database_fetch_row_mysql(void *state);
#      define RUMBLE_DB_RESULT    0
#   else
#      define RUMBLE_DB_RESULT    100
#   endif
#   include <sqlite3.h>

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Server specific functions
 -----------------------------------------------------------------------------------------------------------------------
 */

void    rumble_pop3_populate(sessionHandle *session, accountSession *pops);
void    rumble_database_update_domains(void);
#endif /* DATABASE_H */
