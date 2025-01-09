#include <ev.h>
#include <stdlib.h>


#include "miscadmin.h"
#include "postgres.h"
#include "utils/elog.h"

#include "config.h"
#include "multiplexer.h"
#include "socket_wrapper.h"

#define close_with_check(socket_fd) \
        int err = close(socket_fd); \
        if (err == -1) { \
            char* err_msg  = strerror(errno); \
            ereport(ERROR, errmsg("on_accept_cb: close error -  %s", err_msg)); \
        }


void close_connection(EV_P_ struct ev_io* io_handle) {
    socket_data* data = io_handle->data;
    close_with_check(io_handle->fd);

    client_req* cur_req = data->read_data->reqs->first;
    while(cur_req != NULL) {
        client_req* req = cur_req->next;
        free(cur_req);
        cur_req = req;
    }

    free(data->read_data->reqs);
    free(data->write_data->answer);
    free(data->read_data->parsing.parsing_str);
    free(data->read_data->read_buffer);
    free(data->read_data);
    free(data->write_data);
    free(data->read_io_handle);
    free(data->write_io_handle);
    free(data);
}

static void on_write_cb(EV_P_ struct ev_io* io_handle, int revents) {}

/*
 * This function is called when data arrives for reading in the corresponding socket.
 * All operations are performed until all incoming data is read, which is necessary to process all requests.
 */
static void on_read_cb(EV_P_ struct ev_io* io_handle, int revents) {
    socket_data* data = (socket_data*)io_handle->data;
    socket_read_data* r_data = data->read_data;
    if (revents & EV_ERROR) {
        ereport(ERROR, errmsg("on_read_cb: EV_ERROR, close connection "));
        close_connection(loop, io_handle);
        return;
    }

    int free_buffer_size = r_data->buffer_size - r_data->cur_buffer_size;
    int res = read(io_handle->fd, r_data->read_buffer + r_data->cur_buffer_size, free_buffer_size);
    if (res > 0) {
        r_data->cur_buffer_size = res;
        Eexit_status status =  pars_data(r_data);
        if (status == ERR) {
            close_connection(loop, io_handle);
            return;
        } else if (status == ALL) {
            while (status == ALL) {
                status = pars_data(r_data);
            }
        } else if (status == NOT_ALL) {
            return;
        }
    } else if (res == 0) {
        ereport(WARNING, errmsg("on_read_cb: read warning, connection reset"));
        close_connection(loop, io_handle);
        return;
    } else if (res < 0) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_read_cb: read error - %s", err_msg));
        close_connection(loop, io_handle);
        return;
    }

}

//When a client connects, a socket is created and two watchers are set up:
//the first one for reading is always active, and the second one for writing is activated only when data needs to be written.
//If there are any issues with connecting a new client, we don't want the entire proxy to break,
//so we simply log a failure message.
static void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents) {
    char* read_buffer;
    init_worker_conf* conf = (init_worker_conf*)io_handle->data;
    int socket_fd;
    requests* reqs;
    socket_data* data;
    socket_read_data* r_data;
    socket_write_data* w_data;
    struct ev_io* read_io_handle;
    struct ev_io* write_io_handle;

    socket_fd = accept(conf->listen_port, NULL, NULL);
    if(socket_fd == -1){
        char* err_msg  = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: accept error - %s", err_msg));
        return;
    }

    data = (socket_data*)malloc(sizeof(socket_data));
    if(data == NULL){
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        return;
    }

    read_io_handle = (struct ev_io*)malloc(sizeof(struct ev_io));
    if (read_io_handle == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        free(data);
        return;
    }

    write_io_handle = (struct ev_io*)malloc(sizeof(struct ev_io));
    if (write_io_handle == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        free(data);
        free(read_io_handle);
        return;
    }

    w_data = (socket_write_data*)malloc(sizeof(socket_write_data));
    if (w_data == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        free(data);
        free(read_io_handle);
        free(write_io_handle);
    }

    r_data = (socket_read_data*)malloc(sizeof(socket_read_data));
    if (w_data == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        free(data);
        free(w_data);
        free(read_io_handle);
        free(write_io_handle);
    }

    read_buffer = malloc(conf->buffer_size * sizeof(char));
    if (read_buffer == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        free(data);
        free(w_data);
        free(r_data);
        free(read_io_handle);
        free(write_io_handle);
    }

    reqs = malloc(sizeof(requests));
    if (reqs == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb: malloc error - %s", err_msg));
        close_with_check(socket_fd)
        free(data);
        free(r_data);
        free(read_buffer);
        free(read_io_handle);
        free(w_data);
        free(write_io_handle);
    }
    reqs->first = reqs->last = NULL;
    reqs->count_req = 0;

    data->write_data = w_data;
    data->write_data->answer = NULL;
    data->write_data->size_answer = 0;
    data->write_io_handle = write_io_handle;

    data->read_data = r_data;
    data->read_data->cur_buffer_size = 0;
    data->read_data->cur_count_argv = 0;
    data->read_data->exit_status = NOT_ALL;
    data->read_data->parsing.cur_size_str = 0;
    data->read_data->parsing.is_negative = false;
    data->read_data->parsing.parsing_num = 0;
    data->read_data->parsing.parsing_str = NULL;
    data->read_data->parsing.size_str = 0;
    data->read_data->read_buffer = read_buffer;
    data->read_data->read_status = ARRAY_WAIT;
    data->read_data->reqs = reqs;
    data->read_io_handle = read_io_handle;

    read_io_handle->data = (void*)data;
    write_io_handle->data = (void*)data;

    ev_io_init(read_io_handle, on_read_cb, socket_fd, EV_READ);
    ev_io_init(write_io_handle, on_write_cb, socket_fd, EV_WRITE);
    ev_io_start(loop, read_io_handle);
}
