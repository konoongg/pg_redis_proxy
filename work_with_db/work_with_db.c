#include "libpq-fe.h"
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#include "work_with_db.h"

PGconn* conn;
PGresult* res;

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
    return 0;
}

get_value_result
get_value(char* table, char* key, char** value){
    char SELECT[200];
    int n_rows;
    if (sprintf(SELECT, "SELECT value FROM %s WHERE key=\'%s\'", table, key) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQfinish(conn);
        return err;
    }
    res = PQexec(conn, SELECT);
    ereport(LOG, errmsg("select send %s", SELECT));
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("select table failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return err;
    }
    n_rows = PQntuples(res);
    ereport(LOG, errmsg("COUNT ROWS %d count column: %d", n_rows, PQnfields(res)));
    if(n_rows > 1){
        ereport(ERROR, errmsg("get_value: more than one value"));
        return err;
    }
    else if(n_rows == 0){
        return  non;

    }
    *value = PQgetvalue(res, 0, 0);
    return ok;
}

void
finish_work_with_db(void){
    ereport(LOG, errmsg("finish work with db"));
    PQclear(res);
    PQfinish(conn);
}