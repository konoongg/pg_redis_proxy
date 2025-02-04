#include <stdio.h>
#include <unistd.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "config.h"
#include "connection.h"
#include "db.h"
#include "resp_creater.h"
#include "command_processor.h"

extern config_redis config;

backend_pool* backends;
req_queue* req_wait_plan;

void free_db(void);
void free_req(req_to_db* req);
void init_db(void);
void* start_db_worker(void*);

void free_db(void) {
    for (int i = 0 ; i < backends->count_backend; ++i ) {
        PQfinish(backends->connection[i]->conn);
    }
}



void free_req(req_to_db* req) {
    free(req->req);
    free(req);
}

void init_db(void) {
    int conn_info_size;
    char* conn_info;
    db_conn_conf* conf = &config.db_conf;

    req_wait_plan = wcalloc(sizeof(req_queue));
    req_wait_plan->first = req_wait_plan->last = NULL;

    req_wait_plan->lock = wcalloc(sizeof(pthread_mutex_t));
    req_wait_plan->cond_has_requests = wcalloc(sizeof(pthread_cond_t));

    backends = wcalloc(sizeof(backend_pool));
    backends->count_backend = conf->count_connection;
    backends->connection = wcalloc(conf->count_connection * sizeof(backend_connection*));

    conn_info_size = CONN_INFO_DEFAULT_SIZE + strlen(conf->dbname) + strlen(getlogin());
    conn_info = wcalloc(conn_info_size * sizeof(char));
    if (sprintf(conn_info, "user=%s dbname=%s host=localhost", getlogin(), conf->dbname) < 0) {
        ereport(INFO, errmsg("init_db: can't create connection info"));
        abort();
    }

    for (int i = 0; i < backends->count_backend; ++i) {
        backends->connection[i] = wcalloc(sizeof(backend_connection));
        backends->connection[i]->conn = PQconnectdb(conn_info);

        if (backends->connection[i]->conn  == NULL) {
            ereport(INFO, errmsg("init_db: PQconnectStart error"));
            free_db();
            abort();
        }

        if (PQstatus(backends->connection[i]->conn) == CONNECTION_BAD) {
            ereport(INFO, errmsg("init_db: PQstatus is bad - %s",  PQerrorMessage(backends->connection[i]->conn)));
            free_db();
            abort();
        }

        backends->connection[i]->fd = PQsocket(backends->connection[i]->conn);
        if (backends->connection[i]->fd == -1) {
            ereport(INFO, errmsg("init_db: PQsocket error"));
            free_db();
            abort();
        }

        backends->connection[i]->status = FREE;
    }

    if (pthread_mutex_init(req_wait_plan->lock, NULL) != 0) {
        ereport(INFO, errmsg("init_db: pthread_mutex_init() failed"));
        free_db();
        abort();
    }

    if (pthread_cond_init(req_wait_plan->cond_has_requests, NULL) != 0) {
        ereport(INFO, errmsg("init_db: pthread_cond_init() failed"));
        free_db();
        abort();
    }
    req_wait_plan->queue_size = 0;
}


db_data* pars_db_data(PGresult* res) {
    db_data* data = wcalloc(sizeof(db_data));
    data->count_rows = PQntuples(res);
    data->count_column = PQnfields(res);

    data->tuples = wcalloc(data->count_rows * sizeof(char**));
    data->size_value = wcalloc(data->count_rows * sizeof(int*));

    for (int row = 0; row < data->count_rows; ++row) {
        data->tuples[row] = wcalloc(data->count_column * sizeof(char*));
        data->size_value[row] = wcalloc(data->count_column * sizeof(int));
        for (int column = 0; column < data->count_column; ++column) {
            data->tuples[row][column] = PQgetvalue(res, row, column);
            data->size_value[row][column] = PQgetlength(res, row, column);
        }
    }
    return data;
}

