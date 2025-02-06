#include "errno.h"
#include <eventfd.h>
#include <pthread.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "connection.h"
#include "db.h"
#include "hash.h"
#include "query_cache_controller.h"

db_worker dbw;
extern config_redis config;

void register_command(command_to_db* cmd) {
    char sig_ev = 0;
    int err = pthread_spin_lock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    if (dbw.commands->first == NULL) {
        dbw.commands->first = dbw.commands->last = cmd;
    } else {
        dbw.commands->last->next = cmd;
        dbw.commands->last = dbw.commands->last->next;
    }
    dbw.commands->count_commands++;

    int res = write(dbw.wthrd->efd, &sig_ev, 1);

    if (res == -1) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("register_command: write %s", err));
        abort();
    }

    err = pthread_spin_unlock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

command_to_db* get_command(void) {
    int err = pthread_spin_lock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    command_to_db*  cmd =  dbw.commands->first;
    dbw.commands->first = dbw.commands->first->next;
    dbw.commands->count_commands--;

    if (dbw.commands->count_commands == 0) {
        dbw.commands->first = dbw.commands->last = NULL;
    }

    int err = pthread_spin_unlock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_unlock %s", strerror(err)));
        abort();
    }

    return cmd;
}
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


proc_status process_write_db(connection* conn) {
    int* id_back = (int*)conn->data;
    command_to_db* cmd = conn->w_data->data;
    db_oper_res res = write_to_db(id_back, cmd->cmd);
    if (res == WRITE_OPER_RES) {
        conn->proc = process_read_db;
        conn->status = READ_DB;
        delete_active(conn);
        add_wait(conn);
    } else if (res == WAIT_OPER_RES) {
        delete_active(conn);
        add_wait(conn);
    } else  if (res == ERR_OPER_RES) {
        free_connection(dbw.backends);
        abort();
    }
}

proc_status process_read_db(connection* conn) {
    int* id_back = (int*)conn->data;
    command_to_db* cmd = conn->w_data->data;
    db_oper_res res = read_from_db(id_back, cmd->cmd);
    if (res == READ_OPER_RES) {
        backend* b = conn->data;
        b->is_free = true;

        delete_active(conn);
        add_wait(conn);
    } else if (res == WAIT_OPER_RES) {
        delete_active(conn);
        add_wait(conn);
    } else  if (res == ERR_OPER_RES) {
        free_connection(dbw.backends);
        abort();
    }
}

proc_status notify_db(connection* conn) {
    char code;
    int res = read(conn->fd, &code, 1);

    if (res < 0 && res != EAGAIN) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("notify: read error %s", err));
        abort();
    } else if (res == 0 || res == EAGAIN) {
        return ALIVE_PROC;
    }

    for (int i = 0; i < dbw.count_backends; ++i) {
        if (dbw.backends[i].is_free) {
            dbw.backends[i].is_free = false;
            delete_wait(dbw.backends[i].conn);
            add_active(dbw.backends[i].conn);
            dbw.backends[i].conn->w_data = get_command();
            dbw.backends[i].conn->data = &(dbw.backends[i]);
            dbw.backends[i].conn->proc = process_write_db;
            dbw.backends[i].conn->status = WRITE_DB;
            break;
        }
    }

    return WAIT_PROC;
}

cache_data* init_cache_data(char* key, int key_size, req_table* args) {
    cache_data* data = wcalloc(sizeof(cache_data));
    data->key = wcalloc(key_size * sizeof(char));
    data->key_size = key_size;
    data->values = wcalloc(sizeof(values));
    data->values->count_attr = args->count_args;
    data->values->attr = wcalloc(args->count_args * sizeof(attr));
    for (int i = 0; i < args->count_args; ++i) {
        column* c = get_column_info(args->table, args->args[i].column_name);
        req_column* attr = &(data->values->attr[i]);
        attr->type = c->type;
        memcpy(attr->data, args->attr[i].data, args->attr[i].data_size);
    }
    return data;
}

void free_cache_data(cache_data* data) {
    free(data);
}

void* start_db_worker(void*) {
    connection* efd_conn;
    dbw.count_backends = config.db_conf.count_backend;
    dbw.backends = wcalloc(dbw.count_backends * sizeof(backend));
    init_db(dbw.backends);

    int err = pthread_spin_init(&(dbw.lock), PTHREAD_PROCESS_PRIVATE);
    if (err != 0) {
        ereport(INFO, errmsg("start_db_worker: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    dbw.commands = wcalloc(sizeof(list_command));
    dbw.commands->first = dbw.commands->last = NULL;

    dbw.wthrd = wcalloc(sizeof(wthread));
    init_wthread(dbw.wthrd);
    init_loop(dbw.wthrd);

    dbw.wthrd->efd = eventfd(0, EFD_NONBLOCK);
    if (wthrd.efd == -1) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("start_db_worker: eventfd error %s", err));
        abort();
    }

    efd_conn = create_connection(dbw.wthr->efd, dbw.wthrd);
    init_event(efd_conn, listen_efd_connconn->r_data->handle, efd_conn->fd, EVENT_READ);
    add_wait(efd_conn);
    efd_conn->proc = notify_db;
    efd_conn->status = NOTIFY_DB;


    for (int i = 0; i < dbw.count_backends; ++i) {
        connection* db_conn = create_connection(dbw.backends[i].fd, dbw.wthrd);
        init_event(db_conn, db_conn->r_data->handle, db_conn->fd, EVENT_READ);
        init_event(db_conn, db_conn->w_data->handle, db_conn->fd, EVENT_WRITE);
        add_wait(db_conn);
        dbw.backends[i].conn = db_conn;
    }

    while (true) {
        CHECK_FOR_INTERRUPTS();
        loop_run(dbw.wthrd->l);
        loop_step(dbw.wthrd);
    }
}

void init_db_worker(void) {
    pthread_t db_tid;
    int err = pthread_create(&(db_tid), NULL, start_db_worker, NULL);
    if (err) {
        ereport(INFO, errmsg("init_worker: pthread_create error %s", strerror(err)));
        abort();
    }
}
