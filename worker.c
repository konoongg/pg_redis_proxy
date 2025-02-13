#include <assert.h>
#include <errno.h>
#include <eventfd.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/eventfd.h>
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
#include "io.h"
#include "socket_wrapper.h"
#include "worker.h"


int init_workers(void);
proc_status process_accept(connection* conn);
proc_status process_data(connection* conn);
proc_status process_read(connection* conn);
proc_status process_write(connection* conn);
void finish_connection(connection* conn);
void* start_worker(void* argv);

extern config_redis config;
thread_local wthread wthrd;

proc_status process_write(connection* conn) {
    event_data* w_data = conn->write_data;
    answer_list* answers  = (answer_list*)w_data->data;
    answer* cur_answer = answers->first;

    while (cur_answer != NULL) {
        int res = write(io_handle->fd, cur_answer->answer, cur_answer->answer_size);
        if (res == cur_answer->answer_size) {
            answer* next_answer = cur_answer->next;
            free_answer(cur_answer);
            cur_answer = next_answer;
            answers->first = cur_answer;
        } else if (res == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return ALIVE_PROC;
            }

            delete_active(conn);
            finish_connection(conn);
            return DEL_PROC;
        } else {
            cur_answer->answer_size -=  res;
            memmove(cur_answer->answer, cur_answer + res, cur_answer->answer_size);
            return ALIVE_PROC;
        }
    }
    answers->first = answers->last = NULL;

    start_event(conn->wthrd->l, conn->r_data->handle);
    stop_event(conn->wthrd->l, conn->w_data->handle);


    move_from_active_to_wait(conn);
    return WAIT_PROC;
}

proc_status notify(connection* conn) {
    char code;
    int res = read(conn->fd, &code, 1);

    if (res < 0 && res != EAGAIN) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("notify: read error %s", err));
        abort();
    } else if (res == 0 || res == EAGAIN) {
        return ALIVE_PROC;
    }

    move_from_active_to_wait(conn);
    return WAIT_PROC;
}

proc_status process_data(connection* conn) {
    io_read* r_data = (io_read*)conn->r_data->data;
    answer_list* w_data = (answer_list*)conn->w_data->data;
    client_req* cur_req;
    answer* cur_answer;

     while (true) {
        process_result res;
        cur_req = r_data->reqs->first;
        if (cur_req == NULL) {
            conn->status = WRITE;
            conn->proc = process_write;
            start_event(conn->wthrd->l, conn->w_data->handle);

            move_from_active_to_wait(conn);
            return WAIT_PROC;
        }

        if (w_data->first == NULL) {
            w_data->last = w_data->first = wcalloc(sizeof(answer));
        } else {
            w_data->last->next = wcalloc(sizeof(answer));
            w_data->last = w_data->last->next;
        }
        cur_answer = w_data->last;

        res = process_command(cur_req, cur_answer, conn);
        if (res == DONE) {
            r_data->reqs->first = r_data->reqs->first->next;
            free_req(cur_req);
        } else if (res == DB_REQ) {
            // process_command moved conenction
            return WAIT_PROC;
        } else if  (res == PROCESS_ERR) {
            abort();
        }
    }
}

proc_status process_read(connection* conn) {
    exit_status status;
    int buffer_free_size;
    int res;
    io_read* r_data = (io_read*)conn->r_data->data;

    buffer_free_size = r_data->buffer_size - r_data->cur_buffer_size;
    res = read(conn->fd, r_data->read_buffer + r_data->cur_buffer_size, buffer_free_size);

    if (res > 0) {
        r_data->cur_buffer_size = res;
        status = pars_data(r_data);
        if (status == ERR) {
            conn->status = CLOSE;
            delete_active(conn);
            finish_connection(conn);
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
            move_from_active_to_wait(conn);
            return WAIT_PROC;
        }
    } else if (res == 0) {
        conn->status = CLOSE;
        delete_active(conn);
        finish_connection(conn);
        return DEL_PROC;

    } else if (res < 0) {
        conn->status = CLOSE;
        delete_active(conn);
        finish_connection(conn);
        return DEL_PROC;
    }
}

proc_status process_accept(connection* conn) {
    int fd;
    connection* new_conn;
    char* read_buffer;
    requests* reqs;

    fd = accept(wthrd->listen_socket, NULL, NULL);
    if (fd == -1) {
        abort();
    }
    new_conn = create_connection(fd, &wthrd);

    io_read* io_r = wcalloc(sizeof(io_r));
    answer_list* a_list = wcalloc(sizeof(answer_list));

    read_buffer = wcalloc(config.worker_conf.buffer_size * sizeof(char));
    reqs = wcalloc(sizeof(requests));
    reqs->first = reqs->last = NULL;
    reqs->count_req = 0;

    a_list->first = a_list->last = NULL;


    io_r->cur_buffer_size = 0;
    io_r->pars.cur_count_argv = 0;
    io_r->pars.cur_size_str = 0;
    io_r->pars.parsing_num = 0;
    io_r->pars.parsing_str = NULL;
    io_r->pars.size_str = 0;
    io_r->read_buffer = read_buffer;
    io_r->pars.cur_read_status = ARRAY_WAIT;
    io_r->reqs = reqs;

    new_conn->r_data->data = io_r;
    new_conn->w_data->data = a_list;

    new_conn->r_data->free_data = io_read_free;
    new_conn->w_data->free_data = answer_

    add_wait(new_conn);

    init_event(new_conn, new_conn->r_data->handle, new_conn->fd, EVENT_READ);
    init_event(new_conn, new_conn->w_data->handle, new_conn->fd, EVENT_WRITE);

    new_conn->status = READ;
    new_conn->proc = process_read;

    start_event(wthrd->l, r_data->handle);

    move_from_active_to_wait(conn);
    return WAIT_PROC;
}


void* start_worker(void* argv) {
    bool run;
    connection* listen_conn;
    connection* efd_conn;


    int listen_socket = init_listen_socket(config.worker_conf.listen_port, config.worker_conf.backlog_size);
    if (wthrd.listen_socket == -1) {
        abort();
    }

    init_wthread(&wthrd);
    wthrd.l = init_loop();


    listen_conn = create_connection(listen_socket, &wthrd);
    init_event(listen_conn, listen_conn->r_data->handle, listen_conn->fd, EVENT_READ);
    add_wait(listen_conn);
    listen_conn->proc = process_accept;
    listen_conn->status = ACCEPT;

    start_event(wthrd->l, listen_conn->r_data->handle);

    wthrd.efd = eventfd(0, EFD_NONBLOCK);
    if (wthrd.efd == -1) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("start_worker: eventfd error %s", err));
        abort();
    }

    efd_conn = create_connection(wthrd.efd, &wthrd);
    init_event(efd_conn, listen_efd_connconn->r_data->handle, efd_conn->fd, EVENT_READ);
    add_wait(efd_conn);
    efd_conn->proc = notify;
    efd_conn->status = NOTIFY;

    start_event(wthrd->l, efd_conn->r_data->handle);

    while (true) {
        CHECK_FOR_INTERRUPTS();
        loop_run(wthrd.l);
        loop_step(&wthrd);
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
