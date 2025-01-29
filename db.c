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

int register_command(req_to_db* new_req, db_connect* db_conn) {
    if (pthread_mutex_lock(req_wait_plan->lock)) {
        return -1;
    }

    if (req_wait_plan->first == NULL) {
        ereport(INFO, errmsg("register_command: req_wait_plan->first == NULL"));
        req_wait_plan->first = req_wait_plan->last = new_req;
    } else {
        ereport(INFO, errmsg("register_command: req_wait_plan->first != NULL"));
        req_wait_plan->last->next = new_req;
        req_wait_plan->last = req_wait_plan->last->next;
    }
    ereport(INFO, errmsg("register_command: new_req->req %s", new_req->req));
    req_wait_plan->last->next = NULL;
    req_wait_plan->queue_size++;


    ereport(INFO, errmsg("register_command: req_wait_plan->last %p", req_wait_plan->last));
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

    ereport(INFO, errmsg("register_command: register"));
    return db_conn->pipe_to_db[1];
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

void* start_db_worker(void*) {
    int count_column;
    int count_rows;
    PGresult* res = NULL;
    req_to_db* cur_req;

    init_db();

    while(true) {

        int err = pthread_mutex_lock(req_wait_plan->lock);
        if (err != 0) {
            char* msg_err = strerror(err);
            ereport(INFO, errmsg("start_db_worker: pthread_mutex_lock error %s", msg_err));
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

        err = pthread_mutex_unlock(req_wait_plan->lock);
        if (err != 0) {
            abort();
        }
        ereport(INFO, errmsg("start_db_worker: cur_req->req_size %d",cur_req->size_req));

        for (int i = 0; i < cur_req->size_req; ++i) {
            ereport(INFO, errmsg("start_db_worker: cur_req->req[%d] %d %c", i, cur_req->req[i], cur_req->req[i]));
        }

        res = PQexec(backends->connection[0]->conn, cur_req->req);

        if (res == NULL) {
            ereport(INFO, errmsg("start_db_worker: res == NULL PQexec %s", PQerrorMessage(backends->connection[0]->conn)));
            abort();
        }

        if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
            ereport(INFO, errmsg("start_db_worker: PQresultStatus %d  error PQexec %s", PQresultStatus(res), PQerrorMessage(backends->connection[0]->conn)));
            abort();
        }

        if (cur_req->reason == WAIT_DATA) {
            answer* full_answer;
            answer** answers;
            answer* data;

            db_data* pg_data = pars_db_data(res);
            answers  = wcalloc(pg_data->count_rows * sizeof(answer*));

            for (int i = 0; i < pg_data->count_rows; ++i) {
                answers[i] = wcalloc(sizeof(answer*));
                create_array_bulk_string_resp(answers[i], pg_data->count_column, pg_data->tuples[i], pg_data->size_value[i]);


                ereport(INFO, errmsg("start_db_worker:  answers[%d]->answer_size %d",i, answers[i]->answer_size));

                for (int j = 0; j < answers[i]->answer_size; ++j) {
                    ereport(INFO, errmsg("start_db_worker:  answers[%d]->answer[%d] %d %c",
                        i, j,answers[i]->answer[j], answers[i]->answer[j] ));
                }
            }

            full_answer = wcalloc(sizeof(answer));
            init_array_by_elems(full_answer, pg_data->count_rows, answers);
            data = cur_req->data;

            data->answer = wcalloc(data->answer_size  * sizeof(char));
            memcpy(data->answer, full_answer->answer, data->answer_size);;

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
