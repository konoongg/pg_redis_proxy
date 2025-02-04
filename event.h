#ifndef EV_H
#define EV_H

#include "connection.h"
#include "ev.h"

typedef struct event_loop event_loop;

void init_event(void* data, handle* h, int fd);
void init_loop(wthread* wthrd);
void start_event(event_loop* l, handle* h);
void stop_event(event_loop* l, handle* h);
void loop_run(event_loop* l);
void loop_destroy(event_loop* l);

struct event_loop {
    void* loop;
};

#endif