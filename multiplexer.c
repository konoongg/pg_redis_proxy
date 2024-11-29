#include <ev.h>
#include <stdlib.h>

#include "postgres.h"
#include "utils/elog.h"
#include "miscadmin.h"


#include "socket_wrapper.h"
#include "multiplexer.h"

static void on_accept_cb(EV_P_ struct ev_io *io_handle, int revents);

struct ev_loop *loop = NULL;

//free the resources needed by the multiplexer
int finish_multiplexer(void) {
    ev_loop_destroy(loop);
    return 0;
}

//When a client connects, a socket is created and two watchers are set up:
//the first one for reading is always active, and the second one for writing is activated only when data needs to be written.
//If there are any issues with connecting a new client, we don't want the entire proxy to break,
//so we simply log a failure message.
static void on_accept_cb(EV_P_ struct ev_io *io_handle, int revents) {
    int socket_fd = -1;
    struct ev_io *read_io_handle;
    struct ev_io *write_io_handle;
    socket_data *sock_data = (socket_data *) malloc(sizeof(socket_data));
    if (sock_data == NULL) {
        ereport(WARNING, errmsg("can't alloc memory, can't accept connection"));
        return;
    }
    socket_fd = init_socket();
    if(socket_fd == -1){
        ereport(WARNING, errmsg("can't create socket for new connection"));
        return;
    }
}


int run_multiplexer(int listen_port, int baclog_size) {
    struct ev_io *accept_io_handle;
    int listen_socket = init_listen_socket(listen_port, baclog_size);
    if (listen_socket == -1) {
        return -1;
    }
    loop = ev_default_loop(0);
    if (loop == NULL) {
        ereport(ERROR, errmsg("cannot create libev default loop"));
        return -1;
    }
    accept_io_handle = (struct ev_io *) malloc(sizeof(struct ev_io));
    if (accept_io_handle == NULL) {
        ereport(ERROR, errmsg("cannot create io handle for listen socket"));
        return -1;
    }
    ev_io_init(accept_io_handle, on_accept_cb, listen_socket, EV_READ);
    ev_io_start(loop, accept_io_handle);
    for (;;) {
        CHECK_FOR_INTERRUPTS();
        ev_run(loop, EVRUN_ONCE);
    }
    return 0;
}
