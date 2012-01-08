RADB
====
**RADB** is a simple C/C++ SQL layer for using the same API calls for different engines (currently supporting MySQL and SQLite3).
All connection handles and objects are handled by the layer, and prepared statements can be accessed through the printf-styled functions.
You can get the latest source at either sourceforge or at github. 

A quick example:
----------------

    #include "radb.h"
    void regular_C_func(void) {
        /*~~~~~~~~~~~~~~~*/
        radbMaster* db;
        radbObject* dbo;
        radbResult* result;
        int rc;
        /*~~~~~~~~~~~~~~~*/
        /* Initialize the database connection */
        db = radb_init_sqlite("mydatabase.db");
     
        /* Run a simple query */
        radb_run(db, "DELETE FROM `myTable` WHERE age < 10");
     
        /* Run a statement with injected values */
        dbo = radb_prepare(\
            db,\
            "SELECT city, country, numberofpets FROM `someTable` WHERE name = %s and age = %u",\
            "JohnDoe's Mother", 97\
        );
        while ((result = radb_fetch_row(dbo))) {
            printf("City: %s, country: %s, no. of pets: %u\n", \
                result->column[0].data.string, \
                result->column[1].data.string, \
                result->column[2].data.uint32 );
     
        }
        /* Clean up results and close the handle */
        radb_cleanup(dbo);
     
        /* Close the database connection */
        radb_close(db);
    }
     
    void c_plus_plus_version(void) {
        radb* db = new radb();
        db->init_sqlite("mydb");
        db->run("DELETE FROM `stuff` WHERE 1");
        radbo* dbo = db->prepare("SELECT * FROM `someTable`");
        while ((radbResult* result = dbo->fetch_row()) {
            print("stuff: %s\n", result->column[0].data.string);
        }
        /* Deleting the classes is enough to clean up the mess */
        delete dbo;
        delete db;
    }


Formatted statements
--------------------
SQL Statements can be used with injected values through a printf-like system.
The following tags are supported:

* %s: A string value (`char*` or `const char*`)
* %u: An unsigned 32 bit integer (`unsigned int` or `uint32_t`)
* %d: A signed 32 bit integer (`signed int` or `int32_t`)
* %l: A signed 64 bit integer (`signed long long` int or `int64_t`)
* %f: A 64 bit floating point value (`double`)


###Using formatted statements
The functions `radb_run_inject` and `radb_prepare` support formatted statements:

    /* Run a simple query */
    radb_run_inject(db, "SELECT * FROM `test` WHERE id = %u and name = %s", 12345, "Some Name");

    /* Prepare a statement for multiple injections */
    radbObject* dbo = radb_prepare("SELECT * FROM `test` WHERE id = %u and name = %s");
    radb_inject(dbo, 12345, "Blah blah blah");
    int hits = radb_query(dbo);

    /* Inject some other values and rerun the query */
    radb_inject(dbo, 54321, "Some other guy");
    hits = radb_query(dbo);


Public functions
----------------

* `radbMaster* radb_init_sqlite(const char* filename)`:
    Opens up an SQLite3 database file and returns a pointer to the radbMaster struct handling it.
    
* `radbMaster *radb_init_mysql(unsigned threads, const char *host, const char *user, const char *pass, const char *db, unsigned port)`:
    Opens up the number of connections specified by `threads` to the MySQL host and returns a pointer to the radbMaster struct handling it.

* `int radb_run(radbMaster *radbm, const char *statement)`:
    Runs the SQL statement and returns either the number of rows affected or returned.

* `int radb_run_inject(radbMaster *radbm, const char *statement, ...)`:
    Runs the formatted SQL statement and returns either the number of rows affected or returned.

* `radbObject  *radb_prepare(radbMaster *radbm, const char *statement, ...)`:
    Prepares an SQL statement. 
    If additional values are supplied, they are injected into the statement as the initial values.

* `int radb_inject(radbObject *dbo, ...)`:
    Injects new values into an already prepared statement.

* `int radb_query(radbObject *dbo)`:
    Executes the prepared statement and returns the number of rows affected or returned.

* `radbResult  *radb_fetch_row(radbObject *dbo)`:
    Fetches the next row of results (if any) and returns it.
    If no results are left, it returns 0.

* `void    radb_cleanup(radbObject *dbo)`:
    Cleans up an SQL query so you don't have to.

* `void    radb_close(radbMaster *dbm)`:
    Shuts down the database connection and frees any existing handles.
    
Structure of the results object
-------------------------------

* `radbResult` ->
* * items (`unsigned int`): The number of columns in the returned row.
* * column[] (`radbItem*`) ->
* * * type (`enum (STRING=1,NUMBER=2)`): The type of data returned
* * * data (`union`): A union struct with all types of returnable data:
* * * * string (`char*`): Return data as a string pointer
* * * * uint32 (`uint32_t` or unsigned int): Return value as a 32 bit unsigned int.
* * * * int32 (`int32_t` or signed int): Return value as a 32 bit signed int.
* * * * int64 (`int64_t` or signed long long int): Return value as a 64 bit signed int.
* * * * uint64 (`uint64_t` or unsigned long long int): Return value as a 64 bit unsigned int.
* * * * double (`double`): Return value as a 64 bit floating point value.
* * * * float (`float`): Return value as a 32 bit floating point value.

    

Some final examples
-------------------

## Creating a table and inserting data

    static const char* names[] = { "Abe Lincoln", "Moe Szyslak", "Jane Doe" };
    
    void RunSomeStatements(void) {
        
        /*~~~~~~~~~~~~*/
        int rc;
        radbObject* obj;
        radbMaster* db;
        /*~~~~~~~~~~~~*/
        
        /* Open the connection to the db */
        db = radb_init_mysql(5, "127.0.0.1", "username", "somepassword", "somedatabase");
        if (!db) { fprintf(stderr, "Something went wrong :(\n"); exit(0); }
        
        /* Run some statements */
        radb_run("CREATE TABLE `someTable` (`id` MEDIUMINT UNSIGNED, `name` VARCHAR( 128 ) NOT NULL);");
        
        /* Insert all our names into the new table */
        obj = radb_prepare("INSERT INTO `someTable` (`id, `name`) VALUES (%d, %s)");
        for (rc = 0;names[rc]; rc++ ) {
            /* Inject values and call the query function to execute the statement */
            radb_inject(rc+1, names[rc]);
            radb_query(obj);
        }
        
        /* Close up */
        radb_cleanup(obj);
        radb_close(db);
    }
        

## Retrieving data from a table

    void getNames(void) {
        /*~~~~~~~~~~~~*/
        int rc;
        radbObject* obj;
        radbMaster* db;
        /*~~~~~~~~~~~~*/
        
        /* Open the connection to the db */
        db = radb_init_sqlite("somefile.sqlite");
        
        /* Fire off a query to fetch some rows */
        obj = radb_prepare("SELECT `name`, `id` FROM `someTable`");
        rc = radb_query(obj); /* rc = Did we get any hits? */
        
        if (rc > 0) {
            radbResult* result;
            for (result = radb_fetch_row(obj); result != NULL; result = radb_fetch_row(obj)) {
                printf("ID: %d - Name: %s\n", result->column[1].data.int32, result->column[0].data.string);
            }
        }
        else printf("We got no results!\n");
        radb_cleanup(obj);
        radb_close(db);
    }
        