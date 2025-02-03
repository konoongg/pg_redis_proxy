#ifndef EV_H
#define EV_H

#include "connection.h"
#include "ev.h"

typedef struct event_loop event_loop;

void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents);
void on_read_cb(EV_P_ struct ev_io* io_handle, int revents);
void init_loop(wthread* wthrd);

struct event_loop {
    struct ev_loop* loop;
};

#endif