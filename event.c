#include <ev.h>

#include "postgres.h"
#include "utils/elog.h"

#include "connection.h"
#include "event.h"

// /*
//  * This function is called when data arrives for reading in the corresponding socket.
//  * All operations are performed until all incoming data is read, which is necessary to process all requests.
//  */
// void on_read_cb(EV_P_ struct ev_io* io_handle, int revents) {
//     exit_status status;
//     int free_buffer_size;
//     int res;
//     socket_data* data = (socket_data*)io_handle->data;
//     socket_read_data* r_data = data->read_data;

//     if (revents & EV_ERROR) {
//         close_connection(loop, io_handle);
//         abort();
//     }

//     free_buffer_size = r_data->buffer_size - r_data->cur_buffer_size;
//     res = read(io_handle->fd, r_data->read_buffer + r_data->cur_buffer_size, free_buffer_size);

//     if (res > 0) {
//         r_data->cur_buffer_size = res;
//         status = pars_data(r_data);
//         if (status == ERR) {
//             close_connection(loop, io_handle);
//             return;
//         } else if (status == ALL) {
//             while (status == ALL) {
//                 status = pars_data(r_data);
//             }
//             ev_io_stop(loop, io_handle);
//             process_req(loop, data);
//             return;
//         } else if (status == NOT_ALL) {
//             return;
//         }
//     } else if (res == 0) {
//         close_connection(loop, io_handle);
//         return;
//     } else if (res < 0) {
//         close_connection(loop, io_handle);
//         return;
//     }
// }

// //When a client connects, a socket is created and two watchers are set up:
// //the first one for reading is always active, and the second one for writing is activated only when data needs to be written.
// //If there are any issues with connecting a new client, we don't want the entire proxy to break,
// //so we simply log a failure message.
void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents) {
    wthread* wthrd = (wthread*)io_handle->data;
    int err = pthread_spin_lock(wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("on_accept_cb: pthread_spin_lock() failed: %s\n", strerror(err)));
	    abort();
    }


    int err = pthread_spin_unlock(wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("on_accept_cb: pthread_spin_unlock() failed: %s\n", strerror(err)));
	    abort();
    }
//     accept_conf* conf = (accept_conf*)io_handle->data;
//     char* read_buffer;
//     int socket_fd;
//     requests* reqs;
//     socket_data* data;
//     socket_read_data* r_data;
//     socket_write_data* w_data;
//     struct ev_io* read_db_handle;
//     struct ev_io* read_io_handle;
//     struct ev_io* write_io_handle;

//     socket_fd = accept(conf->listen_socket, NULL, NULL);
//     if (socket_fd == -1) {
//         abort();
//     }

//     data = (socket_data*)wcalloc(sizeof(socket_data));
//     read_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
//     read_db_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
//     write_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
//     w_data = (socket_write_data*)wcalloc(sizeof(socket_write_data));
//     r_data = (socket_read_data*)wcalloc(sizeof(socket_read_data));

//     read_buffer = wcalloc(conf->buffer_size * sizeof(char));
//     reqs = wcalloc(sizeof(requests));
//     reqs->first = reqs->last = NULL;
//     reqs->count_req = 0;

//     data->write_data = w_data;
//     data->write_data->answers = wcalloc(sizeof(answer_list));
//     data->write_data->answers->first = data->write_data->answers->last = NULL;
//     data->write_io_handle = write_io_handle;

//     data->read_data = r_data;
//     data->read_data->cur_buffer_size = 0;
//     data->read_data->buffer_size = conf->buffer_size;
//     data->read_data->parsing.cur_count_argv = 0;
//     data->read_data->parsing.cur_size_str = 0;
//     data->read_data->parsing.parsing_num = 0;
//     data->read_data->parsing.parsing_str = NULL;
//     data->read_data->parsing.size_str = 0;
//     data->read_data->read_buffer = read_buffer;
//     data->read_data->parsing.cur_read_status = ARRAY_WAIT;
//     data->read_data->reqs = reqs;
//     data->read_io_handle = read_io_handle;

//     read_io_handle->data = (void*)data;
//     write_io_handle->data = (void*)data;
//     read_db_handle->data = (void*)data;

//     ev_io_init(read_io_handle, on_read_cb, socket_fd, EV_READ);
//     ev_io_init(write_io_handle, on_write_cb, socket_fd, EV_WRITE);
//     ev_io_start(loop, read_io_handle);
}

void init_loop(wthread* wthrd) {
    struct ev_io* accept_io_handle;

    wthrd->l->loop = ev_loop_new(ev_recommended_backends());
    if (wthrd->l->loop  == NULL) {
        ereport(INFO, errmsg("init_loop: cannot create libev default loop"));
        abort();
    }

    accept_io_handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    accept_io_handle->data = wthrd;
    ev_io_init(accept_io_handle, on_accept_cb, wthrd->listen_socket, EV_READ);
    ev_io_start(wthrd->l, accept_io_handle);
}
