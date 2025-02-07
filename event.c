#include <assert.h>
#include <ev.h>

#include "postgres.h"
#include "utils/elog.h"

#include "config.h"
#include "connection.h"
#include "event.h"

#include "config.h"
#include "connection.h"

extern config_redis config;

void callback(EV_P_ struct ev_io* io_handle, int revents);

void init_event(void* data, handle* h, int fd, event_mode mode) {
    int flag;
    if (mode == EVENT_READ) {
        flag = EV_READ;
    } else if (mode == EVENT_WRITE) {
        flag = EV_WRITE;
    }

    h->handle = (struct ev_io*)wcalloc(sizeof(struct ev_io));
    ev_io_init((struct ev_io*)h->handle, callback, fd, flag);
    ((struct ev_io*)(h->handle))->data = data;
}

void start_event(event_loop* l, handle* h) {
    ev_io_start(l, (struct ev_io*)h->handle);
}

void stop_event(event_loop* l, handle* h) {
    ev_io_stop(l, (struct ev_io*)h->handle);
}

void init_loop(wthread* wthrd) {
    wthrd->l->loop = ev_loop_new(ev_recommended_backends());
    if (wthrd->l->loop  == NULL) {
        ereport(INFO, errmsg("init_loop: cannot create libev default loop"));
        abort();
    }
}

void loop_run(event_loop* l) {
    bool run = ev_run((ev_loop*)l->loop, EVRUN_ONCE);
    if (!run) {
        ereport(INFO, errmsg("loop_run: ev_run return false"));
        abort();
    }
}

void loop_destroy(event_loop* l) {
    ev_loop_destroy((ev_loop*)l->loop);
}

void callback(EV_P_ struct ev_io* io_handle, int revents) {
    connection* conn = (connection*)io_handle->data;
    wthread* wthrd = conn->wthrd;

    if (revents & EV_ERROR) {
        close_connection(loop, io_handle);
        abort();
    }

    assert(conn->is_wait);
    move_from_wait_to_active(conn);
}
