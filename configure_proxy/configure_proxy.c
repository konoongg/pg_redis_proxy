#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include <netinet/in.h>
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include "libpq-fe.h"

#include "configure_proxy.h"

struct proxy_status* proxy_configure;

/*
 * After execution of this function, extension will start listening on port
 */
int
init_redis_listener(void){
    int listen_socket;
    int client_socket;
    int opt;
    struct sockaddr_in sockaddr;
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == -1) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        return -1 ;
    }
    opt = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        return -1;
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(DEFAULT_PORT);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("bind() error: %s %d", err, listen_socket));
        close(listen_socket);
        return -1;
    }
    if(listen(listen_socket, DEFAULT_BACKLOG_SIZE)) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("listen() error: %s", err));
        close(listen_socket);
        return -1;
    }
    client_socket = accept(listen_socket, NULL, NULL);
    return client_socket;
}

char
check_table(char* new_table_name,  PGresult* res){
    int n_rows = PQntuples(res);
    for (int i = 0; i < n_rows; i++) {
        char* table_name = PQgetvalue(res, i, 0);
        ereport(LOG, errmsg("%d: check Table name: %s and new table name: %s", i, table_name, new_table_name));
        if(strcmp(table_name, new_table_name) == 0){
            return 1;
        }
    }
    ereport(LOG, errmsg("new table name: %s not exist",  new_table_name));
    return 0;
}

int
init_proxy_status(void){
    proxy_configure = (proxy_status*) malloc(sizeof(proxy_status));
    strcpy(proxy_configure->cur_table, "redis_0");
    return 0;
}

char*
get_cur_table(void){
    return proxy_configure->cur_table;
}

int
init_table(void){
    char connect[200];
    char GET_TABLES[] = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'";
    PGconn* conn;
    PGresult* res;
    ereport(LOG, errmsg("start init table"));
    //по-хорошему тут надо првоерять, что все символу записались
    if (sprintf(connect, "user=%s dbname=postgres", getlogin()) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQfinish(conn);
        return -1;
    }
    conn = PQconnectdb(connect);
    if (PQstatus(conn) == CONNECTION_BAD) {
        ereport(ERROR, errmsg( "Connection to database failed: %s", PQerrorMessage(conn)));
        PQfinish(conn);
        return -1;
    }
    ereport(LOG, errmsg( "Connection to database"));
    res = PQexec(conn, GET_TABLES);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("Query execution failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        return -1;
    }
    ereport(LOG, errmsg( "GET_TABLES"));
    for(int i = 0; i < DEFAULT_DB_COUNT; ++i){
        char new_table_name[12];
        //по-хорошему тут надо првоерять, что все символу записались
        if (sprintf(new_table_name, "redis_%d", i) < 0) {
            ereport(ERROR, errmsg( "sprintf err"));
            PQfinish(conn);
            return -1;
        }
        if(check_table(new_table_name, res) == 0){
            char CREATE_TABLE[100];
            ereport(LOG , errmsg( "non db %s - create", new_table_name));
            //по-хорошему тут надо првоерять, что все символу записались
            if (sprintf(CREATE_TABLE, "CREATE TABLE %s (key TEXT, value TEXT)", new_table_name) < 0) {
                ereport(ERROR, errmsg( "sprintf err"));
                PQfinish(conn);
                return -1;
            }
            res = PQexec(conn, CREATE_TABLE);
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                ereport(ERROR, errmsg("creating table failed: %s", PQerrorMessage(conn)));
                PQclear(res);
                PQfinish(conn);
                return -1;
            }
        }
    }
    PQclear(res);
    PQfinish(conn);
    return 0;
}
