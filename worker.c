#include <ev.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>

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
#include "socket_wrapper.h"
#include "worker.h"

#define close_with_check(socket_fd) \
        if (close(socket_fd) == -1) { \
            abort();\
        }

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
void close_connection(EV_P_ struct ev_io* io_handle);
void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents);
void on_read_cb(EV_P_ struct ev_io* io_handle, int revents);
void on_read_db_cb(EV_P_ struct ev_io* io_handle, int revents);
void on_write_cb(EV_P_ struct ev_io* io_handle, int revents);
void process_req(EV_P_ socket_data* data);
void* start_worker(void* argv);

extern config_redis config;

void close_connection(EV_P_ struct ev_io* io_handle) {
    socket_data* data = io_handle->data;
    client_req* cur_req;
    answer* cur_answer;

    close_with_check(io_handle->fd);

    cur_req = data->read_data->reqs->first;
    while(cur_req != NULL) {
        client_req* req = cur_req->next;
        free_req(cur_req);
        cur_req = req;
    }

    cur_answer = data->write_data->answers->last;
    while(cur_answer != NULL) {
        answer* next_answer = cur_answer->next;
        free_answer(cur_answer);
        cur_answer = next_answer;
    }

    ev_io_stop(loop, data->write_io_handle);
    ev_io_stop(loop, data->read_io_handle);
    ev_io_stop(loop, data->read_db_handle);

    free(data->read_data->reqs);
    free(data->read_data->parsing.parsing_str);
    free(data->read_data->read_buffer);
    free(data->read_data);
    free(data->write_data);
    free(data->read_io_handle);
    free(data->write_io_handle);
    free(data);
}


void on_write_cb(EV_P_ struct ev_io* io_handle, int revents) {
    ereport(INFO, errmsg("on_write_cb: start"));
    socket_data* data = (socket_data*)io_handle->data;
    socket_write_data* w_data = data->write_data;
    answer* cur_answer = w_data->answers->first;
    ereport(INFO, errmsg("on_write_cb: cur_answer %p cur_answer->answer %p cur_answer->answer_size %d",
            cur_answer, cur_answer->answer, cur_answer->answer_size));

    ereport(INFO, errmsg("on_write_cb: &(cur_answer->answer_size) %p",
            &(cur_answer->answer_size)));

    if (revents & EV_ERROR) {
        close_connection(loop, io_handle);
        abort();
    }

    ereport(INFO, errmsg("on_write_cb"));

    while (cur_answer != NULL) {
        ereport(INFO, errmsg("on_write_cb: cur_answer->answer %s, cur_answer->answer_size %d",
                    cur_answer->answer, cur_answer->answer_size));
        int res =  write(io_handle->fd, cur_answer->answer, cur_answer->answer_size);
        if (res == cur_answer->answer_size) {
            answer* next_answer = cur_answer->next;
            free_answer(cur_answer);
            cur_answer = next_answer;
            w_data->answers->last = cur_answer;
        } else if (res == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            close_connection(loop, io_handle);
            return;
        } else {
            cur_answer->answer_size -=  res;
            memmove(cur_answer->answer, cur_answer + res, cur_answer->answer_size);
            return;
        }
    }
    w_data->answers->first = w_data->answers->last = NULL;


    ev_io_start(loop, data->read_io_handle);
    ev_io_stop(loop, io_handle);
}

void on_read_db_cb(EV_P_ struct ev_io* io_handle, int revents) {
    ereport(INFO, errmsg("on_read_db_cb: start"));
    socket_data* data = io_handle->data;
    socket_write_data* w_data = data->write_data;
    socket_read_data* r_data = data->read_data;

    answer* cur_answer;
    client_req* cur_req = r_data->reqs->first;
    int res;
    result_db status;

    if (revents & EV_ERROR) {
        close_connection(loop, io_handle);
        abort();
    }



    res = read(io_handle->fd, &status, 1);

    if (res != 1) {
        abort();
    }

    close(io_handle->fd);

    ev_io_stop(loop, io_handle);
    if (status == CONN_DB_OK) {
        //
    } else if (status == CONN_DB_ERR) {
        cur_answer = w_data->answers->last;
        process_err(cur_answer, "ERR");
    }
    free_req(cur_req);
    r_data->reqs->first = r_data->reqs->first->next;
    process_req(loop, data);
}

void process_req(EV_P_ socket_data* data) {

    ereport(INFO, errmsg("process_req: start"));
    socket_read_data* r_data = data->read_data;
    socket_write_data* w_data = data->write_data;
    client_req* cur_req;
    answer* cur_answer;

    while (true) {
        process_result res;

        cur_req = r_data->reqs->first;
        if (cur_req == NULL) {
            ereport(INFO, errmsg("process_req: cur_req == NULL"));
            ev_io_start(loop, data->write_io_handle);
            return;
        }
        if (w_data->answers->first == NULL) {
            w_data->answers->last = w_data->answers->first = wcalloc(sizeof(answer));
        } else {
            w_data->answers->last->next = wcalloc(sizeof(answer));
            w_data->answers->last = w_data->answers->last->next;
        }
        cur_answer = w_data->answers->last;
        ereport(INFO, errmsg("process_req: cur_answer %p cur_answer->answer %p", cur_answer, cur_answer->answer));
        res = process_command(cur_req, cur_answer, data->db_conn);
        if (res == DONE) {
            ereport(INFO, errmsg("process_req: DONE"));
            free_req(cur_req);
            r_data->reqs->first = r_data->reqs->first->next;
        } else if (res == DB_REQ) {
            ereport(INFO, errmsg("process_req: DB_REQ data->db_conn->pipe_to_db[0] %d", data->db_conn->pipe_to_db[0]));

            ev_io_init(data->read_db_handle, on_read_db_cb, data->db_conn->pipe_to_db[0], EV_READ);
            ev_io_start(loop, data->read_db_handle);
            return;
        } else if  (res == PROCESS_ERR) {
            abort();
        }

        cur_answer = w_data->answers->last;
    }
}

/*
 * This function is called when data arrives for reading in the corresponding socket.
 * All operations are performed until all incoming data is read, which is necessary to process all requests.
 */
void on_read_cb(EV_P_ struct ev_io* io_handle, int revents) {
    exit_status status;
    int free_buffer_size;
    int res;
    socket_data* data = (socket_data*)io_handle->data;
    socket_read_data* r_data = data->read_data;

    if (revents & EV_ERROR) {
        close_connection(loop, io_handle);
        abort();
    }

    free_buffer_size = r_data->buffer_size - r_data->cur_buffer_size;
    res = read(io_handle->fd, r_data->read_buffer + r_data->cur_buffer_size, free_buffer_size);

    if (res > 0) {
        r_data->cur_buffer_size = res;
        status = pars_data(r_data);
        if (status == ERR) {
            close_connection(loop, io_handle);
            return;
        } else if (status == ALL) {
            while (status == ALL) {
                status = pars_data(r_data);
            }
            ev_io_stop(loop, io_handle);
            process_req(loop, data);
            return;
        } else if (status == NOT_ALL) {
            return;
        }
    } else if (res == 0) {
        close_connection(loop, io_handle);
        return;
    } else if (res < 0) {
        close_connection(loop, io_handle);
        return;
    }
}

//When a client connects, a socket is created and two watchers are set up:
//the first one for reading is always active, and the second one for writing is activated only when data needs to be written.
//If there are any issues with connecting a new client, we don't want the entire proxy to break,
//so we simply log a failure message.
void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents) {

    accept_conf* conf = (accept_conf*)io_handle->data;
    char* read_buffer;
    int socket_fd;
    requests* reqs;
    socket_data* data;
    socket_read_data* r_data;
    socket_write_data* w_data;
    struct ev_io* read_db_handle;
    struct ev_io* read_io_handle;
    struct ev_io* write_io_handle;

    socket_fd = accept(conf->listen_socket, NULL, NULL);
    if (socket_fd == -1) {
        abort();
    }

    data = (socket_data*)wcalloc(sizeof(socket_data));
    read_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    read_db_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    write_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    w_data = (socket_write_data*)wcalloc(sizeof(socket_write_data));
    r_data = (socket_read_data*)wcalloc(sizeof(socket_read_data));

    read_buffer = wcalloc(conf->buffer_size * sizeof(char));
    reqs = wcalloc(sizeof(requests));
    reqs->first = reqs->last = NULL;
    reqs->count_req = 0;

    data->read_db_handle = read_db_handle;
    data->db_conn = wcalloc(sizeof(db_connect));

    data->write_data = w_data;
    data->write_data->answers = wcalloc(sizeof(answer_list));
    data->write_data->answers->first = data->write_data->answers->last = NULL;
    data->write_io_handle = write_io_handle;

    data->read_data = r_data;
    data->read_data->cur_buffer_size = 0;
    data->read_data->buffer_size = conf->buffer_size;
    data->read_data->parsing.cur_count_argv = 0;
    data->read_data->parsing.cur_size_str = 0;
    data->read_data->parsing.parsing_num = 0;
    data->read_data->parsing.parsing_str = NULL;
    data->read_data->parsing.size_str = 0;
    data->read_data->read_buffer = read_buffer;
    data->read_data->parsing.cur_read_status = ARRAY_WAIT;
    data->read_data->reqs = reqs;
    data->read_io_handle = read_io_handle;

    read_io_handle->data = (void*)data;
    write_io_handle->data = (void*)data;
    read_db_handle->data = (void*)data;

    ev_io_init(read_io_handle, on_read_cb, socket_fd, EV_READ);
    ev_io_init(write_io_handle, on_write_cb, socket_fd, EV_WRITE);
    ev_io_start(loop, read_io_handle);
}

void* start_worker(void* argv) {
    //ereport(INFO, errmsg("start worker %d", gettid()));
    accept_conf a_conf;
    bool run;
    init_worker_conf conf = config.worker_conf;
    struct ev_io* accept_io_handle;
    struct ev_loop* loop;
    int listen_socket = init_listen_socket(conf.listen_port, conf.backlog_size);

    if (listen_socket == -1) {
        abort();
    }


    loop = ev_loop_new(ev_recommended_backends());
    if (loop == NULL) {
        //ereport(ERROR, errmsg("cannot create libev default loop"));
        abort();
    }

    accept_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));


    a_conf.buffer_size = conf.buffer_size;
    a_conf.listen_socket = listen_socket;

    accept_io_handle->data = &a_conf;
    ev_io_init(accept_io_handle, on_accept_cb, listen_socket, EV_READ);
    ev_io_start(loop, accept_io_handle);
    while (true) {
        CHECK_FOR_INTERRUPTS();
        run = ev_run(loop, EVRUN_ONCE);
        if (!run) {
            //ereport(ERROR, errmsg("init_data: ev_run return false"));
            ev_loop_destroy(loop);
            abort();
        }
    }

    ev_loop_destroy(loop);
    free(accept_io_handle);
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
