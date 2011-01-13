/* 
 * File:   database.h
 * Author: Administrator
 *
 * Created on January 2, 2011, 5:59 PM
 */

#ifndef DATABASE_H
#define	DATABASE_H

    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <sqlite3.h>
    #include <openssl/sha.h>
    #include <inttypes.h>

    sqlite3_stmt* rumble_sql_inject(sqlite3* db, const char* statement, ...);


#endif	/* DATABASE_H */

