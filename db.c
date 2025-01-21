#include "libpq-fe.h"
#include "postgres.h"
#include "utils/elog.h"


#include "alloc.h"
#include "config.h"
#include "db.h"

backend_pool* backends;
req_queue* req_wait_plan;


void free_db(void) {
    for (int i = 0 ; i < backends->count_backend; ++i ) {
        PQfinish(backends->connection[i]->conn);
    }
}

void register_command() {
    
}

void init_db(db_conn_conf* conf) {
    req_wait_plan = wcalloc(sizeof(req_queue));
    req_wait_plan->first = req_wait_plan->last = NULL;

    backends = wcalloc(sizeof(backend_pool));
    backends->count_backend = conf->count_conneton;
    backends->connection = walloc(conf->count_conneton * sizeof(backend_connection));

    char* conn_info = CONN_INFO_DEFAULT_SIZE + strlen(conf->dbname) + strlen(conf->user);
    if (sprintf(conn_info, "user=%s dbname=%s", conf->user, conf->dbname) < 0) {
        ereport(ERROR, errmsg("init_db: can't create connection info"));
        abort();
    }

    for (int i = 0; i < backends->count_backend; ++i) {
        backends->connection[i] = wcalloc(sizeof(backend_connection));
        backends->connection[i]->conn = PQconnectStart(conn_info);
        if (backends->connection[i]->conn  == NULL) {
            free_db();
            ereport(ERROR, errmsg("init_db: PQconnectStart error"));
            abort();
        }

        if (PQstatus(backends->connection[i]->conn) == CONNECTION_BAD) {
            free_db();
            ereport(ERROR, errmsg("init_db: PQstatus is bad - %s",  PQerrorMessage(backends->connection[i])));
            abort();
        }

        backends->connection[i]->fd = PQsocket(backends->connection[i]);
        if (backends->connection[i]->fd == -1) {
            free_db();
            ereport(ERROR, errmsg("init_db: PQsocket error"));
            abort();
        }

        backends->connection[i]->status = FREE;
    }
}

void* start_db_worker(void* argv) {
    db_conn_conf* conf = (db_conn_conf*)argv;
    init_db(conf);
}

pthread_t init_db_worker(db_conn_conf* conf) {
    pthread_t db_tid;
    int err = pthread_create(&(db_tid), NULL, start_db_worker, conf);
    if (err) {
        ereport(ERROR, errmsg("init_worker: pthread_create error %s", strerror(err)));
        abort();
    }
    return db_tid;
}
