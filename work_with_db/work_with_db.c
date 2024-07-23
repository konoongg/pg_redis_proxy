#include "libpq-fe.h"
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>


#include "work_with_db.h"

PGconn* conn;
PGresult* res = NULL;
bool connected = false;

int
init_work_with_db(void){
    char connect[200];
    if (sprintf(connect, "user=%s dbname=postgres", getlogin()) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQfinish(conn);
        return -1;
    }
    ereport(LOG, errmsg("start work with db"));
    conn = PQconnectdb(connect);
    if (PQstatus(conn) == CONNECTION_BAD) {
        ereport(ERROR, errmsg( "Connection to database failed: %s", PQerrorMessage(conn)));
        PQfinish(conn);
        return -1;
    }
    connected = true;
    return 0;
}

req_result
get_value(char* table, char* key, char** value, int* length){
    char SELECT[200];
    int n_rows;
    if(!connected){
        ereport(ERROR, errmsg("get_value: not connected with bd"));
        return ERR_REQ;
    }
    ereport(LOG, errmsg("SELECT: %s", table));
    if (sprintf(SELECT, "SELECT value FROM %s WHERE key=\'%s\'", table, key) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    res = PQexec(conn, SELECT);
    ereport(LOG, errmsg("select send %s", SELECT));
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("select table failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    n_rows = PQntuples(res);
    ereport(LOG, errmsg("COUNT ROWS %d count column: %d", n_rows, PQnfields(res)));
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

req_result
set_value(char* table, char* key, char* value){
    char INSERT[200];
    if(!connected){
        ereport(ERROR, errmsg("set_value: not connected with bd"));
        return ERR_REQ;
    }
    ereport(LOG, errmsg("INSERT: %s", table));
    if (sprintf(INSERT, "INSERT INTO %s (key, value) VALUES (\'%s\', \'%s\')", table, key, value) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    res = PQexec(conn, INSERT);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("select table failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    return OK;
}

req_result
get_table_name(char*** tables_name, int* n_rows){
    char GET_TABLES[] = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'";
    if(!connected){
        ereport(ERROR, errmsg("set_value: not connected with bd"));
        return ERR_REQ;
    }
    res = PQexec(conn, GET_TABLES);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("Query execution failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    ereport(LOG, errmsg( "GET_TABLES"));
    *n_rows = PQntuples(res);
    (*tables_name) = (char**)malloc(*n_rows * sizeof(char*) );
    for(int i = 0; i < *n_rows; ++i){
        (*tables_name)[i] = PQgetvalue(res, i, 0);
    }
    return OK;
}

req_result
create_table(char* new_table_name){
    char CREATE_TABLE[100];
    ereport(LOG , errmsg( "non db %s - create", new_table_name));
    if (sprintf(CREATE_TABLE, "CREATE TABLE %s (key TEXT, value TEXT)", new_table_name) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQfinish(conn);
        return ERR_REQ;
    }
    res = PQexec(conn, CREATE_TABLE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("creating table failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    return OK;
}

req_result
del_value(char* table, char* key){
    char DELETE[200];
    if(!connected){
        ereport(ERROR, errmsg("del_value: not connected with bd"));
        return ERR_REQ;
    }
    ereport(LOG, errmsg("DELETE: %s", table));
    if (sprintf(DELETE, "DELETE FROM %s  WHERE key= \'%s\'", table, key) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    res = PQexec(conn, DELETE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("select table failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return ERR_REQ;
    }
    if(atoi(PQcmdTuples(res)) == 0){
        return  NON;
    }
    ereport(LOG, errmsg("DELETE: %d with KEY: %s", atoi(PQcmdTuples(res)), key));
    return OK;
}

void
finish_work_with_db(void){
    connected = false;
    ereport(LOG, errmsg("finish work with db"));
    PQclear(res);
    PQfinish(conn);
}