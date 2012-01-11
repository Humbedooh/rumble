/*$I0 */
#ifndef _RADB_H_
#   define _RADB_H_
#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#   include <time.h>
#   include <stdarg.h>
#   if defined(_WIN32) || defined(_WIN64)
#      include <windows.h>
#      pragma warning(disable : 5)
#      pragma warning(disable : 996)
#      include <mysql.h>
#   endif

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Uncomment any of these to disable support
 -----------------------------------------------------------------------------------------------------------------------
 */

/* include <mysql.h> */
#   include <sqlite3.h>
#   define _RADB_H_
#   define RADB_EMPTY      0
#   define RADB_SQLITE3    1
#   define RADB_MYSQL      2
#   define RADB_PARSED     1
#   define RADB_PREPARED   2
#   define RADB_BOUND      3
#   define RADB_EXECUTED   4
#   define RADB_FETCH      5
#   ifndef _STDINT_H
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        _uint32_t;
typedef signed int          int32_t;
typedef unsigned long long  uint64_t;
#      define uint32_t    _uint32_t
#   endif
#   ifndef __int8_t_defined
typedef long long           int64_t;
#   endif
#   if !defined(__ssize_t_defined) && !defined(_SSIZE_T_)
typedef signed int          ssize_t;
#   endif
typedef struct
{
    unsigned    inUse;
    void        *handle;
} radbChild;
typedef struct
{
    radbChild   *children;
    unsigned    count;
} radbPool;
typedef struct
{
    unsigned    dbType;
    radbPool    pool;
    void        *handle;
} radbMaster;

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radbItem: A struct holding exactly one value from one column as a union.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

typedef struct
{
    enum { STRING = 1, NUMBER = 2 }           types;
    unsigned    type;
    unsigned    size;
    union
    {
        char        string[256];
        uint32_t    uint32;
        int32_t     int32;
        int64_t     int64;
        uint64_t    uint64;
        double      _double;
        float       _float;
    } data;
} radbItem;

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radbResult: A result object holding the currently fetched row of data
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

typedef struct
{
    radbItem    *column;
    unsigned    items;
    void        *bindings;
} radbResult;

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radbObject: An object holding the current SQL statement and its status
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

typedef struct
{
    void        *state;
    void        *db;
    unsigned    status;
    char        buffer[1024];
    radbResult  *result;
    radbMaster  *master;
    void        *inputBindings;
    char        inputs[64];
    const char  *lastError;
} radbObject;

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Fixed prototypes
 -----------------------------------------------------------------------------------------------------------------------
 */

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_run: Run a plain SQL command and retrieve the number of rows affected or returned, nothing else. This function
    is a wrapper for opening, querying and closing a database handle.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

signed int  radb_run(radbMaster *radbm, const char *statement);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_run_inject: Same as radb_run, but with a formatted statement with injected values.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

int radb_run_inject(radbMaster *radbm, const char *statement, ...);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_prepare: Initiates a prepared statement with (or without) injected values. If you have injected values (or
    none are needed), you can call radb_query to retrieve the number of rows affected or returned, depending on your
    statement.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

radbObject  *radb_prepare(radbMaster *radbm, const char *statement, ...);
radbObject  *radb_prepare_vl(radbMaster *dbm, const char *statement, va_list vl);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_inject: Injects new values into the prepared statement referenced by dbo.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

int radb_inject(radbObject *dbo, ...);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_inject_vl: Same as radb_inject but with a va_list instead.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

int radb_inject_vl(radbObject *dbo, va_list args);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_query: Runs the prepared statement and returns the number of rows affected of returned (depending on your SQL
    operation)
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

signed int  radb_query(radbObject *dbo);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_step (aka radb_fetch_row): Fetches a result from the active query. If the query hasn't been executed yet.
    radb_step takes care of that as well.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

radbResult  *radb_step(radbObject *dbo);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_free_result: Frees up a result struct. You shouldn't use this unless you know what you're doing - instead, use
    radb_cleanup at the end of your query.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void    radb_free_result(radbResult *result);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_cleanup: Cleans up after a statement has been executed and the results, bindings etc are no longer needed.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void    radb_cleanup(radbObject *dbo);

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_prepare_result: Internal function for preparing the result structure based on the SQL operation
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void        radb_prepare_result(radbObject *dbo);
const char  *radb_last_error(radbObject *dbo);
#   define radb_free       radb_free_result
#   define radb_fetch_row  radb_step
#   define radb_do         radb_run

/*$1
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    radb_close: Shuts down the database connection and frees up the handles etc etc.
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

void    radb_close(radbMaster *dbm);

/*$2
 -----------------------------------------------------------------------------------------------------------------------
    Model-specific definitions
 -----------------------------------------------------------------------------------------------------------------------
 */

#   ifdef _SQLITE3_H_
radbMaster  *radb_init_sqlite(const char *file);
void        radb_prepare_sqlite(radbObject *dbo, const char *statement, va_list vl);
radbResult  *radb_fetch_row_sqlite(radbObject *dbo);
#   endif
#   ifdef MYSQL_CLIENT
radbMaster  *radb_init_mysql(unsigned threads, const char *host, const char *user, const char *pass, const char *db, unsigned port);
void        radb_prepare_mysql(radbObject *dbo, const char *statement, va_list vl);
radbResult  *radb_fetch_row_mysql(radbObject *dbo);
void        radb_release_handle_mysql(radbPool *pool, void *handle);
void        *radb_get_handle_mysql(radbPool *pool);
#      define RUMBLE_DB_RESULT    0
#   else
#      define RUMBLE_DB_RESULT    100
#   endif

/* C++ wrapper */
#   ifdef __cplusplus
class   radbo
{
/*
 -----------------------------------------------------------------------------------------------------------------------
 -----------------------------------------------------------------------------------------------------------------------
 */
public:
    radbo(radbMaster *db);
    radbo(void);
    ~           radbo(void);
    inline int  query(void);
    inline void cleanup(void);
    inline int  inject(...);
    radbResult  *fetch_row(void);
    radbObject  *dbo;
};
class   radb
{
/*
 -----------------------------------------------------------------------------------------------------------------------
 -----------------------------------------------------------------------------------------------------------------------
 */
public:
    ~       radb(void);
#      ifdef _SQLITE3_H_
    void    init_sqlite(const char *filename);
#      endif
#      ifdef MYSQL_CLIENT
    void    init_mysql(unsigned threads, const char *host, const char *user, const char *pass, const char *db, unsigned port);
#      endif
    int     run(const char *statement);
    int     run_inject(const char *statement, ...);
    radbo   *prepare(const char *statement, ...);
    void    disconnect(void);

/*
 -----------------------------------------------------------------------------------------------------------------------
 -----------------------------------------------------------------------------------------------------------------------
 */
protected:
    radbMaster  *dbm;
};
#   endif
#endif
