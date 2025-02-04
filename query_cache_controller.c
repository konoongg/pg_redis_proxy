#include <pthread.h>

#include "alloc.h"
#include "hash.h"
#include "db.h"
#include "query_cache_controller.h"

// int register_command(req_to_db* new_req, db_connect* db_conn) {
//     if (pthread_mutex_lock(req_wait_plan->lock)) {
//         return -1;
//     }

//     if (req_wait_plan->first == NULL) {
//         ereport(INFO, errmsg("register_command: req_wait_plan->first == NULL"));
//         req_wait_plan->first = req_wait_plan->last = new_req;
//     } else {
//         ereport(INFO, errmsg("register_command: req_wait_plan->first != NULL"));
//         req_wait_plan->last->next = new_req;
//         req_wait_plan->last = req_wait_plan->last->next;
//     }
//     ereport(INFO, errmsg("register_command: new_req->req %s", new_req->req));
//     req_wait_plan->last->next = NULL;
//     req_wait_plan->queue_size++;


//     ereport(INFO, errmsg("register_command: req_wait_plan->last %p", req_wait_plan->last));
//     if (pthread_cond_signal(req_wait_plan->cond_has_requests)) {
//         return -1;
//     }

//     if (pthread_mutex_unlock(req_wait_plan->lock)) {
//         return -1;
//     }

//     db_conn->finish_read = false;
//     if (pipe(db_conn->pipe_to_db) == -1) {
//         return -1;
//     }

//     ereport(INFO, errmsg("register_command: register %d %d", db_conn->pipe_to_db[0], db_conn->pipe_to_db[1]));
//     return db_conn->pipe_to_db[1];
// }

// void* start_db_worker(void*) {
//     int count_column;
//     int count_rows;
//     PGresult* res = NULL;
//     req_to_db* cur_req;

//     

//     while(true) {

//         int err = pthread_mutex_lock(req_wait_plan->lock);
//         if (err != 0) {
//             char* msg_err = strerror(err);
//             ereport(INFO, errmsg("start_db_worker: pthread_mutex_lock error %s", msg_err));
//             abort();
//         }

//         while (req_wait_plan->queue_size == 0) {
//             if (pthread_cond_wait(req_wait_plan->cond_has_requests, req_wait_plan->lock) != 0) {
//                 abort();
//             }
//         }
//         cur_req = req_wait_plan->first;

//         req_wait_plan->first = req_wait_plan->first->next;
//         req_wait_plan->queue_size--;

//         err = pthread_mutex_unlock(req_wait_plan->lock);
//         if (err != 0) {
//             abort();
//         }

//         res = PQexec(backends->connection[0]->conn, cur_req->req);

//         if (res == NULL) {
//             ereport(INFO, errmsg("start_db_worker: res == NULL PQexec %s", PQerrorMessage(backends->connection[0]->conn)));
//             abort();
//         }

//         if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
//             ereport(INFO, errmsg("start_db_worker: PQresultStatus %d  error PQexec %s", PQresultStatus(res), PQerrorMessage(backends->connection[0]->conn)));
//             abort();
//         }

//         if (cur_req->reason == WAIT_DATA) {
//             answer* full_answer;
//             answer** answers;
//             answer* data;

//             db_data* pg_data = pars_db_data(res);
//             answers  = wcalloc(pg_data->count_rows * sizeof(answer*));

//             for (int i = 0; i < pg_data->count_rows; ++i) {
//                 answers[i] = wcalloc(sizeof(answer*));
//                 create_array_bulk_string_resp(answers[i], pg_data->count_column, pg_data->tuples[i], pg_data->size_value[i]);
//             }

//             full_answer = wcalloc(sizeof(answer));
//             init_array_by_elems(full_answer, pg_data->count_rows, answers);
//             data = cur_req->data;
//             data->answer_size = full_answer->answer_size;
//             data->answer = wcalloc(data->answer_size  * sizeof(char));
//             memcpy(data->answer, full_answer->answer, data->answer_size);

//             if (lock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
//                 abort();
//             }

//             if (set_cache(create_data(cur_req->key, cur_req->key_size, full_answer, free_resp_answ)) != 0) {
//                 abort();
//             }
//             notify(cur_req->key, cur_req->key_size, CONN_DB_OK);

//             if (unlock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
//                 abort();
//             }
//         } else if (cur_req->reason == WAIT_SYNC) {
//             if (lock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
//                 abort();
//             }

//             notify(cur_req->key, cur_req->key_size, CONN_DB_OK);

//             if (unlock_cache_basket(cur_req->key, cur_req->key_size) == -1) {
//                 abort();
//             }
//         }

//         PQclear(res);
//         free_req(cur_req);
//     }
//     return NULL;
// }

db_meta* meta;

void* start_db_worker(void*) {
    init_db();
    meta = wcalloc(sizeof(db_meta));
    meta->hash_func =  hash_pow_31_mod_100
    meta->attrs = wcalloc(HASH_P_31_M_100_SIZE * sizeof());
    init_meta_db(meta);
}

void init_db_worker(void) {
    pthread_t db_tid;
    int err = pthread_create(&(db_tid), NULL, start_db_worker, NULL);
    if (err) {
        ereport(INFO, errmsg("init_worker: pthread_create error %s", strerror(err)));
        abort();
    }
}