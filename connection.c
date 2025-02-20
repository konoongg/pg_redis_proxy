#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "connection.h"
#include "event.h"

void add(connection* conn, conn_list* list) ;
void delete(connection* conn, conn_list* list);
void free_wthread(wthread* wthrd);

void add(connection* conn, conn_list* list) {
    if (list->first == NULL) {
        conn->prev = NULL;
        conn->next = NULL;
        list->first = list->last = conn;
    } else {
        conn->prev = list->last;
        list->last->next = conn;
        list->last = conn;
    }
}

void delete(connection* conn, conn_list* list) {
    assert(list->first);
    ereport(INFO, errmsg("delete: start conn %p", conn));
    if (conn->prev == NULL) {
        ereport(INFO, errmsg("delete: conn->prev == NULL "));
        list->first = conn->next;
        ereport(INFO, errmsg("delete: list->first %p", list->first));
    } else {
        conn->prev->next = conn->next;
    }

    if (conn->next == NULL) {
        list->last = conn->prev;
        ereport(INFO, errmsg("delete: conn->next == NULL list->last  %p", list->last ));
    }
}

void add_active(connection* conn) {
    int err = pthread_spin_lock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("add_active: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    conn->is_wait = false;
    add(conn, conn->wthrd->active);
    conn->wthrd->active_size++;

    err = pthread_spin_unlock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("add_active: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

void delete_active(connection* conn) {
    int err = pthread_spin_lock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("delete_active: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    delete(conn, conn->wthrd->active);
    conn->wthrd->active_size--;

    err = pthread_spin_unlock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("delete_active: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

void add_wait(connection* conn) {
    ereport(INFO, errmsg("add_wait: conn %p", conn));

    int err = pthread_spin_lock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("add_wait: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    conn->is_wait = true;
    add(conn, conn->wthrd->wait);
    conn->wthrd->wait_size++;

    err = pthread_spin_unlock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("add_wait: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}


void delete_wait(connection* conn) {
    int err = pthread_spin_lock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("delete_wait: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    delete(conn, conn->wthrd->wait);
    conn->wthrd->wait_size--;

    err = pthread_spin_unlock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("delete_wait: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

void move_from_active_to_wait(connection* conn) {
    int err = pthread_spin_lock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("move_from_active_to_wait: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    delete(conn, conn->wthrd->active);
    conn->wthrd->active_size--;
    conn->is_wait = true;
    add(conn, conn->wthrd->wait);
    conn->wthrd->wait_size++;

    err = pthread_spin_unlock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("move_from_active_to_wait: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

void move_from_wait_to_active(connection* conn) {
    int err = pthread_spin_lock(conn->wthrd->lock);
    if (err != 0) {
        ereport(INFO, errmsg("move_from_wait_to_active: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    ereport(INFO, errmsg("move_from_wait_to_active: conn %p", conn));
    delete(conn, conn->wthrd->wait);
    conn->wthrd->wait_size--;
    conn->is_wait = false;
    add(conn, conn->wthrd->active);
    conn->wthrd->active_size++;

    err = pthread_spin_unlock(conn->wthrd->lock);
    if (err != 0) {

        ereport(INFO, errmsg("move_from_wait_to_active: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

connection* create_connection(int fd, wthread* wthrd) {
    event_data* r_data;
    event_data* w_data;
    connection* conn = (connection*)wcalloc(sizeof(connection));

    conn->fd = fd;

    w_data = (event_data*)wcalloc(sizeof(event_data));
    r_data = (event_data*)wcalloc(sizeof(event_data));

    r_data->handle = wcalloc(sizeof(handle));
    w_data->handle = wcalloc(sizeof(handle));

    conn->w_data = w_data;
    conn->r_data = r_data;

    conn->next = NULL;
    conn->prev = NULL;
    conn->wthrd = wthrd;
    return conn;
}

void free_connection(connection* conn) {
    ereport(INFO, errmsg("free_connection:"));
    event_data* r_data = (event_data*)conn->r_data;
    event_data* w_data = (event_data*)conn->w_data;

    r_data->free_data(r_data->data);
    w_data->free_data(w_data->data);

    free(r_data);
    free(w_data);

    if (close(conn->fd) == -1) {
        char* err_msg = strerror(errno);
        ereport(INFO, errmsg("process_close: close() failed: %s\n", err_msg));
        abort();
    }

    free(conn->r_data->handle);
    free(conn->w_data->handle);
    free(conn->r_data);
    free(conn->w_data);
    free(conn);
}

void init_wthread(wthread* wthrd) {
    int err;

    wthrd->active = wcalloc(sizeof(conn_list));
    wthrd->active->first = wthrd->active->last = NULL;
    wthrd->active_size = 0;
    wthrd->wait = wcalloc(sizeof(conn_list));
    wthrd->wait->first = wthrd->wait->last = NULL;
    wthrd->wait_size = 0;

    wthrd->lock = wcalloc(sizeof(pthread_spinlock_t));

    err = pthread_spin_init(wthrd->lock, PTHREAD_PROCESS_PRIVATE);
    if (err != 0) {
        ereport(INFO, errmsg("init_wthread: pthread_spin_lock %s", strerror(err)));
        abort();
    }
}


void loop_step(wthread* wthrd) {
    while (wthrd->active_size != 0) {
        ereport(INFO, errmsg("loop_step: thrd->active->first  %p", wthrd->active->first));
        connection* cur_conn = wthrd->active->first;
        while (cur_conn != NULL) {
            connection* cur_conn_next = cur_conn->next;
            assert(!cur_conn->is_wait);
            cur_conn->proc(cur_conn);
            cur_conn = cur_conn_next;
        }
    }
}

void free_wthread(wthread* wthrd) {
    close(wthrd->listen_socket);
    close(wthrd->efd);
    free(wthrd->active);
    free(wthrd->wait);
    loop_destroy(wthrd->l);
}

void finish_connection(connection* conn) {
    ereport(INFO, errmsg("finish_connection:"));
    stop_event(conn->wthrd->l, conn->r_data->handle);
    stop_event(conn->wthrd->l, conn->w_data->handle);
    free_connection(conn);
}

void event_notify(int fd) {
    char sig_ev = 0;
    int res = write(fd, &sig_ev, 1);
    if (res == -1) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("register_command: write %s", err));
        abort();
    }
}
