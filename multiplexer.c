#include <ev.h>
#include <stdlib.h>
#include <sys/socket.h>


#include "postgres.h"
#include "utils/elog.h"
#include "miscadmin.h"

#include "alloc.h"
#include "command_processor.h"
#include "config.h"
#include "connection.h"
#include "data_parser.h"
#include "multiplexer.h"
#include "socket_wrapper.h"

#define close_with_check(socket_fd) \
        if (close(socket_fd) == -1) { \
            /*char* err_close  = strerror(errno);*/ \
            /*ereport(ERROR, errmsg("on_accept_cb: close error -  %s", err_close));*/ \
        }

void close_connection(EV_P_ struct ev_io* io_handle);
void on_write_cb(EV_P_ struct ev_io* io_handle, int revents);
void on_read_cb(EV_P_ struct ev_io* io_handle, int revents);


void close_connection(EV_P_ struct ev_io* io_handle) {

    ereport(INFO, errmsg("close_connection: close_connection"));
    socket_data* data = io_handle->data;
    client_req* cur_req;
    answer* cur_answer;

    close_with_check(io_handle->fd);

    cur_req = data->read_data->reqs->first;
    while(cur_req != NULL) {
        client_req* req = cur_req->next;
        free(cur_req);
        cur_req = req;
    }

    cur_answer = data->write_data->answers;
    while(cur_answer != NULL) {
        answer* next_answer = cur_answer->next;
        free(cur_answer->answer);
        free(cur_answer);
        cur_answer = next_answer;
    }

    ev_io_stop(loop, data->write_io_handle);
    ev_io_stop(loop, data->read_io_handle);

    free(data->read_data->reqs);
    free(data->read_data->parsing.parsing_str);
    free(data->read_data->read_buffer);
    free(data->read_data);
    free(data->write_data);
    free(data->read_io_handle);
    free(data->write_io_handle);
    free(data);
}

void free_req(client_req* req) {
    for (int i = 0; i < req->argc; ++i) {
        free(req->argv[i]);
    }
    free(req->argv);
    free(req);
}

void on_write_cb(EV_P_ struct ev_io* io_handle, int revents) {
    ereport(INFO, errmsg("on_write_cb: TEST io_handle->fd %d", io_handle->fd));
    socket_data* data = (socket_data*)io_handle->data;
    socket_write_data* w_data = data->write_data;
    answer* cur_answer = w_data->answers;
    while (cur_answer != NULL) {
        ereport(INFO, errmsg("on_write_cb: cur_answer != NULL"));
        int res =  write(io_handle->fd, cur_answer->answer, cur_answer->answer_size);
        ereport(INFO, errmsg("on_write_cb: res %d cur_answer->answer_size %d", res, cur_answer->answer_size));
        if (res == cur_answer->answer_size) {
            answer* next_answer = cur_answer->next;
            free(cur_answer->answer);
            free(cur_answer);
            cur_answer = next_answer;
            w_data->answers = cur_answer;
        } else if (res == -1) {
            //char* err_msg = strerror(errno);
            //ereport(ERROR, errmsg("on_write_cb: write error %s  - ", err_msg));
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
    w_data->answers = NULL;

    ereport(INFO, errmsg("on_write_cb: ev_io_stop"));
    ev_io_stop(loop, io_handle);
    ereport(INFO, errmsg("on_write_cb: FINIHS"));
}

/*
 * This function is called when data arrives for reading in the corresponding socket.
 * All operations are performed until all incoming data is read, which is necessary to process all requests.
 */
void on_read_cb(EV_P_ struct ev_io* io_handle, int revents) {
    ereport(INFO, errmsg("on_read_cb: TEST"));
    client_req* cur_req;
    client_req* next_req;
    answer* cur_answer;
    answer* prev_answer;
    exit_status status;
    int free_buffer_size;
    int res;
    socket_data* data = (socket_data*)io_handle->data;
    socket_read_data* r_data = data->read_data;
    socket_write_data* w_data = data->write_data;

    if (revents & EV_ERROR) {
        //ereport(ERROR, errmsg("on_read_cb: EV_ERROR, close connection "));
        close_connection(loop, io_handle);
        return;
    }

    free_buffer_size = r_data->buffer_size - r_data->cur_buffer_size;
    res = read(io_handle->fd, r_data->read_buffer + r_data->cur_buffer_size, free_buffer_size);

    //ereport(INFO, errmsg("on_read_cb: res %d free_buffer_size %d ", res, free_buffer_size));
    if (res > 0) {
        r_data->cur_buffer_size = res;
        status = pars_data(r_data);
        if (status == ERR) {
            ereport(INFO, errmsg("on_read_cb: status ERR"));
            close_connection(loop, io_handle);
            return;
        } else if (status == ALL) {
            while (status == ALL) {
                ereport(INFO, errmsg("on_read_cb: status ALL"));
                status = pars_data(r_data);
            }

            cur_req = r_data->reqs->first;

            w_data->answers = wcalloc(sizeof(answer));
            w_data->answers->next = NULL;
            cur_answer = w_data->answers;
            prev_answer = NULL;
            while (cur_req != NULL) {
                ereport(INFO, errmsg("on_read_cb: cur_req != NULL"));
                if (cur_answer == NULL) {
                    cur_answer = wcalloc(sizeof(answer));
                    w_data->answers->next = NULL;
                    prev_answer->next = cur_answer;
                }
                process_command(cur_req, cur_answer);
                next_req = cur_req->next;
                free_req(cur_req);
                cur_req = next_req;
                prev_answer = cur_answer;
                cur_answer = cur_answer->next;
            }

            ereport(INFO, errmsg("on_read_cb: start write io_handle->fd %d", io_handle->fd));
            ev_io_start(loop, data->write_io_handle);

        } else if (status == NOT_ALL) {
            ereport(INFO, errmsg("on_read_cb: status NOT ALL"));
            return;
        }
    } else if (res == 0) {
        //ereport(WARNING, errmsg("on_read_cb: read warning, connection reset"));
        close_connection(loop, io_handle);
        return;
    } else if (res < 0) {
        //char* err_msg = strerror(errno);
        //ereport(ERROR, errmsg("on_read_cb: read error - %s", err_msg));
        close_connection(loop, io_handle);
        return;
    }
}

//When a client connects, a socket is created and two watchers are set up:
//the first one for reading is always active, and the second one for writing is activated only when data needs to be written.
//If there are any issues with connecting a new client, we don't want the entire proxy to break,
//so we simply log a failure message.
void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents) {

    ereport(INFO, errmsg("on_accept_cb: TEST"));
    char* read_buffer;
    accept_conf* conf = (accept_conf*)io_handle->data;
    int socket_fd;
    requests* reqs;
    socket_data* data;
    socket_read_data* r_data;
    socket_write_data* w_data;
    struct ev_io* read_io_handle;
    struct ev_io* write_io_handle;

    socket_fd = accept(conf->listen_socket, NULL, NULL);
    if(socket_fd == -1){
        //char* err_msg  = strerror(errno);
        //ereport(ERROR, errmsg("on_accept_cb: accept error - %s", err_msg));
        return;
    }

    data = (socket_data*)wcalloc(sizeof(socket_data));
    read_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    write_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    w_data = (socket_write_data*)wcalloc(sizeof(socket_write_data));
    r_data = (socket_read_data*)wcalloc(sizeof(socket_read_data));
    read_buffer = wcalloc(conf->buffer_size * sizeof(char));
    reqs = wcalloc(sizeof(requests));
    reqs->first = reqs->last = NULL;
    reqs->count_req = 0;

    data->write_data = w_data;
    data->write_data->answers = NULL;
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

    ev_io_init(read_io_handle, on_read_cb, socket_fd, EV_READ);
    ev_io_init(write_io_handle, on_write_cb, socket_fd, EV_WRITE);
    ev_io_start(loop, read_io_handle);
}
