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

void free_req(req_to_db* req) {
    free(req->req);
    free(req->data);
    free(req);
}

void init_db(void) {
    int conn_info_size;
    char* conn_info;
    db_conn_conf* conf = &config.db_conf;

    req_wait_plan = wcalloc(sizeof(req_queue));
    req_wait_plan->first = req_wait_plan->last = NULL;

    backends = wcalloc(sizeof(backend_pool));
    backends->count_backend = conf->count_conneton;
    backends->connection = wcalloc(conf->count_conneton * sizeof(backend_connection*));

    conn_info_size = CONN_INFO_DEFAULT_SIZE + strlen(conf->dbname) + strlen(conf->user);
    conn_info = wcalloc(conn_info_size * sizeof(char));
    if (sprintf(conn_info, "user=%s dbname=%s", conf->user, conf->dbname) < 0) {
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

void* start_db_worker(void*) {
    int count_column;
    int count_rows;
    PGresult* res = NULL;
    req_to_db* cur_req;

    init_db();

    while(true) {

        if (pthread_mutex_lock(req_wait_plan->lock) != 0) {
            abort();
        }

        while (req_wait_plan->queue_size == 0) {
            if (pthread_cond_wait(req_wait_plan->cond_has_requests, req_wait_plan->lock) != 0) {
                abort();
            }
        }

        cur_req = req_wait_plan->first;

        req_wait_plan->first = req_wait_plan->first->next;
        req_wait_plan->queue_size--;

        if (pthread_mutex_unlock(req_wait_plan->lock) != 0) {
            abort();
        }


        res = PQexec(backends->connection[0]->conn, cur_req->req);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            abort();
        }

        if (cur_req->reason == WAIT_DATA) {
            answer* full_answer;
            answer** answers;
            socket_write_data* data;

            count_rows = PQntuples(res);
            count_column = PQnfields(res);
            answers  = wcalloc(count_rows * sizeof(answer*));
            for (int i = 0; i < count_rows; ++i) {
                char** tuple = wcalloc(count_column * sizeof(char*));
                int* tuple_size = wcalloc(count_column * sizeof(int));

                for (int j = 0; j < count_column; ++j) {
                    char* value = PQgetvalue(res, i, j);
                    int  value_size = PQgetlength(res, i, j);
                    tuple[i] = wcalloc(value_size * sizeof(char));
                    memcpy(tuple[i], value, value_size);
                }
                answers[i] = wcalloc(sizeof(answer));
                create_array_bulk_string_resp(answers[i], count_rows, tuple, tuple_size);
            }
            full_answer = wcalloc(sizeof(answer));
            init_array_by_elems(full_answer, count_rows, answers);
            data = cur_req->data;
            data->answers->last->answer = full_answer->answer;
            data->answers->last->answer_size = full_answer->answer_size;
            if (lock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
                abort();
            }

            if (set_cache(create_data(cur_req->key, cur_req->key_size, full_answer, free_resp_answ)) != 0) {
                abort();
            }

            notify(cur_req->key, cur_req->key_size, CONN_DB_OK);

            if (unlock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
                abort();
            }
        } else if (cur_req->reason == WAIT_SYNC) {
             if (lock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
                abort();
            }

            notify(cur_req->key, cur_req->key_size, CONN_DB_OK);

            if (unlock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
                abort();
            }
        }


        PQclear(res);
        free_req(cur_req);
    }
    return NULL;
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
