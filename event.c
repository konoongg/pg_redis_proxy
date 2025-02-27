#include <assert.h>
#include <ev.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "config.h"
#include "connection.h"
#include "event.h"

extern config_redis config;

void callback(EV_P_ struct ev_io* io_handle, int revents);


// Initialization of the event, in the current implementation, libev is used.
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
    ev_io_start(l->loop, (struct ev_io*)h->handle);
}

void stop_event(event_loop* l, handle* h) {
    ev_io_stop(l->loop, (struct ev_io*)h->handle);
}

event_loop* init_loop(void) {
    event_loop* l = wcalloc(sizeof(event_loop));

    l->loop = ev_loop_new(ev_recommended_backends());
    if (l->loop  == NULL) {
        ereport(INFO, errmsg("init_loop: cannot create libev default loop"));
        abort();
    }

    return l;
}

void loop_run(event_loop* l) {
    bool run = ev_run((struct ev_loop*)(l->loop), EVRUN_ONCE);
    if (!run) {
        ereport(INFO, errmsg("loop_run: ev_run return false"));
        abort();
    }
}

void loop_destroy(event_loop* l) {
    ev_loop_destroy((struct ev_loop*)l->loop);
}

/*
* Calling the function to handle the event;
* the connection is moved from the pending queue to the active queue
*/
void callback(EV_P_ struct ev_io* io_handle, int revents) {
    connection* conn = (connection*)io_handle->data;
    if (revents & EV_ERROR) {
        free_connection(conn);
        abort();
    }
    assert(conn->is_wait);
    move_from_wait_to_active(conn);
}
