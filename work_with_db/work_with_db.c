#include "libpq-fe.h"
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>

#include "work_with_db.h"
#include "req_result.h"
#include "../configure_proxy/configure_proxy.h"

PGconn* conn;
PGresult* res = NULL;
bool connected = false;

// this function tries to connect to postgres database as current user
int init_work_with_db(void){
    char connect[200];
    if (sprintf(connect, "user=%s dbname=postgres", getlogin()) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQfinish(conn);
        return -1;
    }
    ereport(INFO, errmsg("start work with db"));
    conn = PQconnectdb(connect);
    if (PQstatus(conn) == CONNECTION_BAD) {
        ereport(ERROR, errmsg( "Connection to database failed: %s", PQerrorMessage(conn)));
        PQfinish(conn);
        return -1;
    }
    connected = true;
    return 0;
}

/*
 * get_value and set_value functions execute SQL queries to get/set data
 * key stores redis-like key, char** value is a pointer to result string (value from db)
 * and length should store length of value + 1 (for \0)
*/
req_result get_value(char* table, char* key, char** value, int* length){
    char SELECT[200];
    int n_rows, sprintf_result;
    if(!connected){
        ereport(ERROR, errmsg("get_value: not connected with db"));
        return ERR_REQ;
    }
    ereport(INFO, errmsg("SELECT: %s", table));
    sprintf_result = sprintf(SELECT, "SELECT h['%s'] FROM %s", key, table);

    if (sprintf_result < 0 || sprintf_result > 2000) {
        ereport(ERROR, errmsg( "sprintf err"));
        return finish_abnormally();
    }
    res = PQexec(conn, SELECT); // execution
    ereport(INFO, errmsg("select send %s", SELECT));
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("select table failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }
    n_rows = PQntuples(res);
    ereport(INFO, errmsg("COUNT ROWS %d count column: %d", n_rows, PQnfields(res)));
    if(n_rows > 1){
        ereport(ERROR, errmsg("get_value: more than one value"));
        return ERR_REQ;
    }
    else if(n_rows == 0){
        return NON;

    }
    *value = PQgetvalue(res, 0, 0);
    *length = PQgetlength(res, 0, 0) + 1; // + \0
    ereport(LOG, errmsg("answer: %ssize: %d", PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0)));
    return OK;
}

req_result set_value(char* table, char* key, char* value){
    char INSERT[200];
    if(!connected){
        ereport(ERROR, errmsg("set_value: not connected with bd"));
        return ERR_REQ;
    }
    ereport(LOG, errmsg("INSERT: %s", table));
    if (sprintf (INSERT, "UPDATE %s SET h['%s']='%s'", table, key, value) < 0) {
        ereport (ERROR, errmsg ("sprintf err"));
        return finish_abnormally();
    }

    res = PQexec(conn, INSERT);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("Setting element in hstore failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }
    return OK;
}

// deletes one key from table
// should return NON if no keys were deleted
req_result del_value(char* table, char* key){
    char FIND[200];
    char DELETE[200];
    int n_rows;
    ereport(LOG, errmsg("del_value: entered function"));
    if (!connected) {
        ereport(ERROR, errmsg("del_value: not connected with db"));
        return ERR_REQ;
    }
    
    // ===== Filling FIND && DELETE with needed requests =====
    if (sprintf(DELETE, "UPDATE %s SET h = delete(h, '%s')", table, key) < 0) {
        ereport(ERROR, errmsg ("sprintf err"));
        return finish_abnormally();
    }
    ereport(LOG, errmsg("Various info: table: %s, key: %s, request: SELECT exist(h, '%s') FROM %s", table, key, key, table));
    if (sprintf(FIND, "SELECT exist(h, '%s') FROM %s;", key, table) < 0) {
        ereport(ERROR, errmsg("spritnf err"));
        return finish_abnormally();
    }

    // ===== Filling ended, execution starts =====
    res = PQexec(conn, FIND);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport (ERROR, errmsg("FINDING ELEMENT IN HSTORE FAILED"));
        return finish_abnormally();
    }

    n_rows = PQntuples(res);
    if (n_rows != 1) {
        ereport(ERROR, errmsg("Incorrect result of exist(h, 'key') command"));
        return finish_abnormally();
    }

    ereport(LOG, errmsg("Received info on '%s' key existence, answer: %s size: %d", key, PQgetvalue(res, 0, 0), PQgetlength(res, 0, 0)));
    if (!strcmp(PQgetvalue(res, 0, 0), "f"))
        return NON;

    res = PQexec(conn, DELETE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport (ERROR, errmsg("Deleting element in hstore failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }

    ereport(LOG, errmsg("Deletion finished"));

    return OK;
}

req_result get_table_name(char*** tables_name, int* n_rows){
    char GET_TABLES[] = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'";
    if(!connected){
        ereport(ERROR, errmsg("set_value: not connected with bd"));
        return ERR_REQ;
    }
    res = PQexec(conn, GET_TABLES);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("Query execution failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }
    ereport(LOG, errmsg( "GET_TABLES"));
    *n_rows = PQntuples(res);
    (*tables_name) = (char**)malloc(*n_rows * sizeof(char*) );
    for(int i = 0; i < *n_rows; ++i){
        (*tables_name)[i] = PQgetvalue(res, i, 0);
    }
    return OK;
}

// creates table in PostgreSQL database.
// h is a key-value structure for strings
req_result create_table(char* new_table_name){
    char CREATE_TABLE[100];
    char CREATE_HSTORE[100];

    // creating table
    ereport(LOG , errmsg( "non db %s - create", new_table_name));
    if (sprintf(CREATE_TABLE, "CREATE TABLE %s (h hstore)", new_table_name) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        return finish_abnormally();
    }
    res = PQexec(conn, CREATE_TABLE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("creating table failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }

    // putting hstore into it
    if (sprintf(CREATE_HSTORE, "INSERT INTO %s VALUES ('')", new_table_name) < 0) {
        ereport(ERROR, errmsg("sprintf err"));
        return finish_abnormally();
    }
    res = PQexec(conn, CREATE_HSTORE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport (ERROR, errmsg("creating hstore failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }
    return OK;
}

void finish_work_with_db(void){
    connected = false;
    ereport(LOG, errmsg("finish work with db"));
    PQclear(res);
    PQfinish(conn);
}

inline req_result finish_abnormally() {
    ereport(ERROR, errmsg("Finished wirk with db abnormally"));
    PQclear(res);
    PQfinish(conn);
    return ERR_REQ;
}