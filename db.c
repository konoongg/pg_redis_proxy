#include "libpq-fe.h"
#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "config.h"
#include "connection.h"
#include "db.h"

extern config_redis config;

backend_pool* backends;
req_queue* req_wait_plan;

void free_db(void) {
    for (int i = 0 ; i < backends->count_backend; ++i ) {
        PQfinish(backends->connection[i]->conn);
    }
}

int register_command(req_to_db* new_req, db_connect* db_conn) {
    if (pthread_mutex_lock(req_wait_plan->lock)) {
        return -1;
    }

    if (req_wait_plan->first == NULL) {
        req_wait_plan->first = req_wait_plan->last = wcalloc(sizeof(req_queue));
    } else {
        req_wait_plan->last->next = wcalloc(sizeof(req_queue));
        req_wait_plan->last = req_wait_plan->last->next;
    }
    req_wait_plan->last = new_req;
    req_wait_plan->last->next = NULL;
    req_wait_plan->queue_size++;

    if (pthread_cond_signal(req_wait_plan->cond_has_requests)) {
        return -1;
    }

    if (pthread_mutex_unlock(req_wait_plan->lock)) {
        return -1;
    }

    db_conn->finish_read = false;
    if (pipe(db_conn->pipe_to_db) == -1) {
        return -1;
    }

    return db_conn->pipe_to_db[1];
}

void init_db(void) {
    db_conn_conf* conf = &config.db_conf;
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

    if (pthread_mutex_init(req_wait_plan->lock, NULL) != 0) {
        free_db();
        ereport(ERROR, errmsg("init_db: pthread_mutex_init() failed"));
        abort();
    }

    if (pthread_cond_init(req_wait_plan->cond_has_requests, NULL)) {
        free_db();
        ereport(ERROR, errmsg("init_db: pthread_cond_init() failed"));
        abort();
    }
    req_wait_plan->queue_size = 0;
}

void* start_db_worker(void) {
    init_db();
}

pthread_t init_db_worker(void) {
    pthread_t db_tid;
    int err = pthread_create(&(db_tid), NULL, start_db_worker, NULL);
    if (err) {
        ereport(ERROR, errmsg("init_worker: pthread_create error %s", strerror(err)));
        abort();
    }
    return db_tid;
}
