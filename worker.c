#include <ev.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "postgres.h"
#include "postmaster/interrupt.h"
#include "utils/elog.h"

#include "miscadmin.h"

#include "config.h"
#include "multiplexer.h"
#include "socket_wrapper.h"
#include "worker.h"


void* start_worker(void* argv);

void* start_worker(void* argv) {
    accept_conf a_conf;
    bool run;
    init_worker_conf* conf = (init_worker_conf*)argv;
    struct ev_io* accept_io_handle;
    struct ev_loop* loop;
    int listen_socket = init_listen_socket(conf->listen_port, conf->backlog_size);

    if (listen_socket == -1) {
        return NULL;
    }

    loop = ev_loop_new(ev_recommended_backends());
    if (loop == NULL) {
        ereport(ERROR, errmsg("cannot create libev default loop"));
        return NULL;
    }

    accept_io_handle = (struct ev_io*) malloc(sizeof(struct ev_io));
    if (accept_io_handle == NULL) {
        ereport(ERROR, errmsg("cannot create io handle for listen socket"));
        ev_loop_destroy(loop);
        return NULL;
    }

    a_conf.buffer_size = conf->buffer_size;
    a_conf.listen_socket = listen_socket;

    accept_io_handle->data = &a_conf;
    ev_io_init(accept_io_handle, on_accept_cb, listen_socket, EV_READ);
    ev_io_start(loop, accept_io_handle);
    while (true) {
        CHECK_FOR_INTERRUPTS();
        run = ev_run(loop, EVRUN_ONCE);
        if (!run) {
            ereport(ERROR, errmsg("init_data: ev_run return false"));
            ev_loop_destroy(loop);
            free(accept_io_handle);
            return NULL;
        }
    }

    ev_loop_destroy(loop);
    free(accept_io_handle);
    return NULL;
}

int init_workers(init_worker_conf* conf) {
    pthread_t* tids = (pthread_t*)malloc(conf->count_worker * sizeof(pthread_t));
    if (tids == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("init_worker: malloc error %s  - ", err_msg));
        return -1;
    }

    for (int i = 0; i < conf->count_worker; ++i) {
        int err = pthread_create(&(tids[i]), NULL, start_worker, conf);
        if (err) {
            ereport(ERROR, errmsg("init_worker: pthread_create error %s", strerror(err)));
            free(tids);
            return -1;
        }
    }

    for (int i = 0; i < conf->count_worker; ++i) {
        int err = pthread_join(tids[i], NULL);
        if (err) {
            ereport(ERROR, errmsg("init_worker: pthread_join error %s", strerror(err)));
            free(tids);
            return -1;
        }
    }

    free(tids);
    return 0;
}
