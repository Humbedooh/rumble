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

void            *rumble_database_prepare(void *db, const char *statement, ...);
void            rumble_pop3_populate(sessionHandle *session, pop3Session *pops);
void            rumble_database_update_domains(void);

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Model-specific definitions
 -----------------------------------------------------------------------------------------------------------------------
 */

#   if RUMBLE_DATABASE_MODEL == RUMBLE_SQLITE3
#      include <sqlite3.h>
#      define rumble_database_run(a)      sqlite3_step((sqlite3_stmt *) a)
#      define rumble_database_cleanup(a)  sqlite3_finalize((sqlite3_stmt *) a)
#      define RUMBLE_DB_RESULT            SQLITE_ROW
#   endif
#endif /* DATABASE_H */
