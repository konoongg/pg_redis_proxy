#include "libpq-fe.h"
#include "postgres.h"
#include "utils/elog.h"


#include "alloc.h"
#include "config.h"
#include "db.h"

backend_pool* backends;

void free_db(void) {
    for (int i = 0 ; i < backends->count_backend; ++i ) {
        PQfinish(backends->connection[i]);
    }
}

int init_db(db_conn_conf* conf) {
    backends = walloc(sizeof(backend_pool));
    backends->count_backend = conf->count_conneton;
    backends->connection = walloc(conf->count_conneton * sizeof(PGconn*));
    backends->connection_fd = walloc(conf->count_conneton * sizeof(int));
    char* conn_info = CONN_INFO_DEFAULT_SIZE + strlen(conf->dbname) + strlen(conf->user);
    if (sprintf(conn_info, "user=%s dbname=%s", conf->user, conf->dbname) < 0) {
        ereport(ERROR, errmsg("init_db: can't create connection info"));
        return -1;
    }

    for (int i = 0; i < backends->count_backend; ++i) {
        backends->connection[i] = PQconnectStart(conn_info);
        if (backends->connection[i]  == NULL) {
            free_db();
            ereport(ERROR, errmsg("init_db: PQconnectStart error"));
            abort();
        }

        if (PQstatus(backends->connection[i]) == CONNECTION_BAD) {
            free_db();
            ereport(ERROR, errmsg("init_db: PQstatus is bad - %s",  PQerrorMessage(backends->connection[i])));
            abort();
        }
        backends->connection_fd[i] = PQsocket(backends->connection[i]);
        if (backends->connection_fd[i] == -1) {
            free_db();
            ereport(ERROR, errmsg("init_db: PQsocket error"));
            abort();
        }
    }
    return 0;
}