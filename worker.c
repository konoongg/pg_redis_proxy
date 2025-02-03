#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <threads.h>

#include "postgres.h"
#include "postmaster/interrupt.h"
#include "utils/elog.h"
#include "miscadmin.h"

#include "alloc.h"
#include "command_processor.h"
#include "config.h"
#include "connection.h"
#include "data_parser.h"
#include "db.h"
#include "event.h"
#include "socket_wrapper.h"
#include "worker.h"

// #define close_with_check(socket_fd) \
//         if (close(socket_fd) == -1) { \
//             abort();\
//         }

// #define free_answer(answ) \
//     if (answ != NULL) { \
//         free(answ->answer); \
//         free(answ); \
//         answ = NULL; \
//     }

// #define free_req(req) \
//     if (req != NULL) { \
//         for (int i = 0; i < req->argc; ++i) { \
//             free(req->argv[i]); \
//         } \
//         free(req->argv); \
//         free(req); \
//         req = NULL; \
//     }

// int init_workers(void);
// void close_connection(EV_P_ struct ev_io* io_handle);
// void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents);
// void on_read_cb(EV_P_ struct ev_io* io_handle, int revents);
// void on_read_db_cb(EV_P_ struct ev_io* io_handle, int revents);
// void on_write_cb(EV_P_ struct ev_io* io_handle, int revents);
// void process_req(EV_P_ socket_data* data);
// void* start_worker(void* argv);

extern config_redis config;
thread_local wthread wthrd;


// void close_connection(EV_P_ struct ev_io* io_handle) {
//     socket_data* data = io_handle->data;
//     client_req* cur_req;
//     answer* cur_answer;

//     close_with_check(io_handle->fd);

//     cur_req = data->read_data->reqs->first;
//     while(cur_req != NULL) {
//         client_req* req = cur_req->next;
//         free_req(cur_req);
//         cur_req = req;
//     }

//     cur_answer = data->write_data->answers->last;
//     while(cur_answer != NULL) {
//         answer* next_answer = cur_answer->next;
//         free_answer(cur_answer);
//         cur_answer = next_answer;
//     }

//     ev_io_stop(loop, data->write_io_handle);
//     ev_io_stop(loop, data->read_io_handle);

//     free(data->read_data->reqs);
//     free(data->read_data->parsing.parsing_str);
//     free(data->read_data->read_buffer);
//     free(data->read_data);
//     free(data->write_data);
//     free(data->read_io_handle);
//     free(data->write_io_handle);
//     free(data);
// }


// void on_write_cb(EV_P_ struct ev_io* io_handle, int revents) {
//     ereport(INFO, errmsg("on_write_cb: start"));
//     socket_data* data = (socket_data*)io_handle->data;
//     socket_write_data* w_data = data->write_data;
//     answer* cur_answer = w_data->answers->first;
//     ereport(INFO, errmsg("on_write_cb: cur_answer %p cur_answer->answer %p cur_answer->answer_size %d",
//             cur_answer, cur_answer->answer, cur_answer->answer_size));

//     ereport(INFO, errmsg("on_write_cb: &(cur_answer->answer_size) %p",
//             &(cur_answer->answer_size)));

//     if (revents & EV_ERROR) {
//         close_connection(loop, io_handle);
//         abort();
//     }

//     ereport(INFO, errmsg("on_write_cb"));

//     while (cur_answer != NULL) {
//         ereport(INFO, errmsg("on_write_cb: cur_answer->answer %s, cur_answer->answer_size %d",
//                     cur_answer->answer, cur_answer->answer_size));
//         int res =  write(io_handle->fd, cur_answer->answer, cur_answer->answer_size);
//         if (res == cur_answer->answer_size) {
//             answer* next_answer = cur_answer->next;
//             free_answer(cur_answer);
//             cur_answer = next_answer;
//             w_data->answers->last = cur_answer;
//         } else if (res == -1) {
//             if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                 return;
//             }
//             close_connection(loop, io_handle);
//             return;
//         } else {
//             cur_answer->answer_size -=  res;
//             memmove(cur_answer->answer, cur_answer + res, cur_answer->answer_size);
//             return;
//         }
//     }
//     w_data->answers->first = w_data->answers->last = NULL;


//     ev_io_start(loop, data->read_io_handle);
//     ev_io_stop(loop, io_handle);
// }

// void on_read_db_cb(EV_P_ struct ev_io* io_handle, int revents) {
//     ereport(INFO, errmsg("on_read_db_cb: start"));
//     socket_data* data = io_handle->data;
//     socket_write_data* w_data = data->write_data;
//     socket_read_data* r_data = data->read_data;

//     answer* cur_answer;
//     client_req* cur_req = r_data->reqs->first;
//     int res;
//     result_db status;

//     if (revents & EV_ERROR) {
//         close_connection(loop, io_handle);
//         abort();
//     }



//     res = read(io_handle->fd, &status, 1);

//     if (res != 1) {
//         abort();
//     }

//     close(io_handle->fd);

//     ev_io_stop(loop, io_handle);
//     if (status == CONN_DB_OK) {
//         //
//     } else if (status == CONN_DB_ERR) {
//         cur_answer = w_data->answers->last;
//         process_err(cur_answer, "ERR");
//     }
//     free_req(cur_req);
//     r_data->reqs->first = r_data->reqs->first->next;
//     process_req(loop, data);
// }

// void process_req(EV_P_ socket_data* data) {

//     ereport(INFO, errmsg("process_req: start"));
//     socket_read_data* r_data = data->read_data;
//     socket_write_data* w_data = data->write_data;
//     client_req* cur_req;
//     answer* cur_answer;

//     while (true) {
//         process_result res;

//         cur_req = r_data->reqs->first;
//         if (cur_req == NULL) {
//             ereport(INFO, errmsg("process_req: cur_req == NULL"));
//             ev_io_start(loop, data->write_io_handle);
//             return;
//         }
//         if (w_data->answers->first == NULL) {
//             w_data->answers->last = w_data->answers->first = wcalloc(sizeof(answer));
//         } else {
//             w_data->answers->last->next = wcalloc(sizeof(answer));
//             w_data->answers->last = w_data->answers->last->next;
//         }
//         cur_answer = w_data->answers->last;
//         ereport(INFO, errmsg("process_req: cur_answer %p cur_answer->answer %p", cur_answer, cur_answer->answer));
//         res = process_command(cur_req, cur_answer, data->db_conn);
//         if (res == DONE) {
//             ereport(INFO, errmsg("process_req: DONE"));
//             free_req(cur_req);
//             r_data->reqs->first = r_data->reqs->first->next;
//         } else if (res == DB_REQ) {
//             ereport(INFO, errmsg("process_req: DB_REQ data->db_conn->pipe_to_db[0] %d", data->db_conn->pipe_to_db[0]));

//             ev_io_init(data->read_db_handle, on_read_db_cb, data->db_conn->pipe_to_db[0], EV_READ);
//             ev_io_start(loop, data->read_db_handle);
//             return;
//         } else if  (res == PROCESS_ERR) {
//             abort();
//         }

//         cur_answer = w_data->answers->last;
//     }
// }


void process_read() {

}

void loop_step(void) {
    int err = pthread_spin_lock(wthrd.lock);
    if (err != 0) {
        ereport(INFO, errmsg("loop_step: pthread_spin_lock() failed: %s\n", strerror(err)));
	    abort();
    }

    connection* cur_conn = wthrd.active->first;

    while (cur_conn != NULL) {
        switch(cur_conn->status) {
            case READED:
                process_read();
                break;
        }
        cur_conn = cur_conn->next;
    }


    err = pthread_spin_unlock(wthrd.lock);
    if (err != 0) {
        ereport(INFO, errmsg("loop_step: pthread_spin_unlock() failed: %s\n", strerror(err)));
	    abort();
    }
}

void free_wthread(void) {
    close(wthrd.listen_socket);
    close(wthrd.efd);
    free(wthrd.active);
    free(wthrd.wait);
    ev_loop_destroy(wthrd.l);

    int err = pthread_spin_destroy(&(wthrd.lock));
    if (err != 0) {
        ereport(INFO, errmsg("free_wthread: pthread_spin_destroy() failed: %s\n", strerror(err)));
        abort();
    }
}

void* start_worker(void* argv) {
    bool run;

    wthrd.listen_socket = init_listen_socket(config.worker_conf.listen_port, config.worker_conf.backlog_size);

    if (wthrd.listen_socket == -1) {
        abort();
    }

    init_loop(&wthrd);
    wthrd.active = wcalloc(sizeof(conn_list));
    wthrd.active->first = wthrd.active->last = NULL;
    wthrd.wait = wcalloc(sizeof(conn_list));
    wthrd.wait->first = wthrd.wait->last = NULL;
    wthrd.active_size = 0;

    wthrd.lock = wcalloc(sizeof(pthread_spinlock_t));
    int err = pthread_spin_init(wthrd.lock, PTHREAD_PROCESS_PRIVATE);
    if  (err != 0) {
        ereport(INFO, errmsg("start_worker: pthread_spin_init error %s", strerror(err)));
        abort();
    }

    while (true) {
        CHECK_FOR_INTERRUPTS();
        run = ev_run(wthrd.l, EVRUN_ONCE);
        if (!run) {
            ereport(INFO, errmsg("start_worker: ev_run return false"));
            abort();
        }
        loop_step();
    }
    return NULL;
}

int init_workers(void) {
    init_worker_conf conf = config.worker_conf;
    pthread_t* tids = wcalloc(conf.count_worker * sizeof(pthread_t));

    for (int i = 0; i < conf.count_worker; ++i) {
        int err = pthread_create(&(tids[i]), NULL, start_worker, NULL);
        if (err) {
            ereport(ERROR, errmsg("init_worker: pthread_create error %s", strerror(err)));
            abort();
        }
    }

    ereport(INFO, errmsg("start all workers (%d)", conf.count_worker));

    for (int i = 0; i < conf.count_worker; ++i) {
        int err = pthread_join(tids[i], NULL);
        if (err) {
            ereport(ERROR, errmsg("init_worker: pthread_join error %s", strerror(err)));
            abort();
        }
    }

    free(tids);
    return 0;
}
