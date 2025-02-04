#include <assert.h>
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

 #define free_answer(answ) \
    if (answ != NULL) { \
        free(answ->answer); \
        free(answ); \
        answ = NULL; \
    }

#define free_req(req) \
    if (req != NULL) { \
        for (int i = 0; i < req->argc; ++i) { \
            free(req->argv[i]); \
        } \
        free(req->argv); \
        free(req); \
        req = NULL; \
    }

int init_workers(void);
proc_status process_accept(connection* conn);
proc_status process_data(connection* conn);
proc_status process_read(connection* conn);
proc_status process_write(connection* conn);
void finish_connection(connection* conn);
void* start_worker(void* argv);

extern config_redis config;
thread_local wthread wthrd;

void finish_connection(connection* conn) {
    event_stop(conn->wthrd, conn->r_data->handle);
    event_stop(conn->wthrd, conn->w_data->handle);
    free_connection(conn);
}

proc_status process_write(connection* conn) {
    write_data* w_data = conn->write_data;
    answer* cur_answer = w_data->answers->first;

    while (cur_answer != NULL) {
        int res = write(io_handle->fd, cur_answer->answer, cur_answer->answer_size);
        if (res == cur_answer->answer_size) {
            answer* next_answer = cur_answer->next;
            free_answer(cur_answer);
            cur_answer = next_answer;
            w_data->answers->last = cur_answer;
        } else if (res == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return ALIVE_PROC;
            }
            finish_connection(conn);
            return DEL_PROC;
        } else {
            cur_answer->answer_size -=  res;
            memmove(cur_answer->answer, cur_answer + res, cur_answer->answer_size);
            return ALIVE_PROC;
        }
    }
    w_data->answers->first = w_data->answers->last = NULL;

    start_event(conn->wthrd->l, conn->r_data->handle);
    stop_event(conn->wthrd->l, conn->w_data->handle);

    return WAIT_PROC;
}

proc_status process_data(connection* conn) {
    read_data* r_data = conn->read_data;
    write_data* w_data = conn->write_data;
    client_req* cur_req;
    answer* cur_answer;

     while (true) {
        process_result res;
        cur_req = r_data->reqs->first;
        if (cur_req == NULL) {
            conn->status = WRITE;
            conn->proc = process_write;
            start_event(conn->wthrd->l, conn->w_data->handle);
            return WAIT_PROC;
        }

        if (w_data->answers->first == NULL) {
            w_data->answers->last = w_data->answers->first = wcalloc(sizeof(answer));
        } else {
            w_data->answers->last->next = wcalloc(sizeof(answer));
            w_data->answers->last = w_data->answers->last->next;
        }
        cur_answer = w_data->answers->last;

        res = process_command(cur_req, cur_answer);
        if (res == DONE) {
            r_data->reqs->first = r_data->reqs->first->next;
            free_req(cur_req);
        } else if (res == DB_REQ) {
            return WAIT_PROC;
        } else if  (res == PROCESS_ERR) {
            abort();
        }
    }
}

proc_status notify(connection* conn) {
    return WAIT_PROC;
}

proc_status process_read(connection* conn) {
    exit_status status;
    int buffer_free_size;
    int res;
    read_data* r_data = conn->read_data;

    buffer_free_size = r_data->buffer_size - r_data->cur_buffer_size;
    res = read(conn->fd, r_data->read_buffer + r_data->cur_buffer_size, buffer_free_size);

    if (res > 0) {
        r_data->cur_buffer_size = res;
        status = pars_data(r_data);
        if (status == ERR) {
            conn->status = CLOSE;
            return DEL_PROC;
        } else if (status == ALL) {
            while (status == ALL) {
                status = pars_data(r_data);
            }
            stop_event(wthrd->l, r_data->handle);
            conn->status = PROCESS;
            conn->proc = process_data;
            return ALIVE_PROC;
        } else if (status == NOT_ALL) {
            return WAIT_PROC;
        }
    } else if (res == 0) {
        conn->status = CLOSE;
        return DEL_PROC;

    } else if (res < 0) {
        conn->status = CLOSE;
        return DEL_PROC;
    }
}

proc_status process_accept(connection* conn) {
    int fd;
    connection* new_conn;

    fd = accept(wthrd->listen_socket, NULL, NULL);
    if (socket_fd == -1) {
        abort();
    }
    new_conn = create_connection(fd);

    init_event(conn, r_data->handle, new_conn->fd);
    init_event(conn, w_data->handle, new_conn->fd);
    new_conn->wthrd = conn->w_data;
    new_conn->status = READ;
    new_conn->proc = process_read;
    start_event(wthrd->l, r_data->handle);
    add_wait(new_conn);
    return WAIT_PROC;
}

void loop_step(void) {
    while (wthrd.active_size != 0) {
        connection* cur_conn = wthrd.active->first;
        while (cur_conn != NULL) {
            assert(!cur_conn->is_wait);
            proc_status status = cur_conn->proc(cur_conn);
            switch(status) {
                case WAIT_PROC:
                    delete_active(cur_conn);
                    add_wait(cur_conn);
                    break;
                case DEL_PROC:
                    delete_active(cur_conn);
                    finish_connection(cur_conn);
                    break;
                case ALIVE_PROC:
                    break;
            }
            cur_conn = cur_conn->next;
        }
    }
}

void free_wthread(void) {
    close(wthrd.listen_socket);
    close(wthrd.efd);
    free(wthrd.active);
    free(wthrd.wait);
    loop_destroy(wthrd.l);
}

void* start_worker(void* argv) {
    bool run;
    connection* listen_conn;

    int listen_socket = init_listen_socket(config.worker_conf.listen_port, config.worker_conf.backlog_size);
    if (wthrd.listen_socket == -1) {
        abort();
    }

    listen_conn = create_connection(listen_socket, &wthrd);
    init_loop(&wthrd);
    listen_conn->status = ACCEPT;

    wthrd.active = wcalloc(sizeof(conn_list));
    wthrd.active->first = wthrd.active->last = NULL;
    wthrd.wait = wcalloc(sizeof(conn_list));
    wthrd.wait->first = wthrd.wait->last = listen_conn;
    wthrd.active_size = 0;
    wthrd.wait_size = 1;

    wthrd.lock = wcalloc(sizeof(pthread_spinlock_t));

    while (true) {
        CHECK_FOR_INTERRUPTS();
        loop_run(wthrd.l);
        loop_step();
    }
    return NULL;
}

void init_workers(void) {
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
}
