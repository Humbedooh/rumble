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
    Server specific functions
 -----------------------------------------------------------------------------------------------------------------------
 */

void    rumble_pop3_populate(sessionHandle *session, accountSession *pops);
void    rumble_database_update_domains(void);
#endif /* DATABASE_H */
