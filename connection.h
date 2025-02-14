#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdbool.h>

#include "event.h"

typedef enum conn_status conn_status;
typedef enum proc_status proc_status;
typedef struct conn_list conn_list;
typedef struct connection connection;
typedef struct event_data event_data;
typedef struct handle handle;
typedef struct wthread wthread;

connection* create_connection(int fd, wthread* wthrd);
void add_active(connection* conn);
void add_wait(connection* conn);
void delete_active(connection* conn);
void delete_wait(connection* conn);
void event_notify(int fd);
void free_connection(connection* conn);
void init_wthread(wthread* wthrd);
void loop_step(wthread* wthrd);
void move_from_active_to_wait(connection* conn);
void move_from_wait_to_active(connection* conn);


struct event_data {
    void* data;
    void (*free_data)(void* data);
    handle* handle;
};

enum conn_status {
    ACCEPT,
    CLOSE,
    READ,
    WRITE,
    WAIT,
    PROCESS,
    NOTIFY,
    NOTIFY_DB,
    READ_DB,
    WRITE_DB,
};

enum proc_status {
    ALIVE_PROC,
    DEL_PROC,
    WAIT_PROC,
};

struct connection {
    bool is_wait;
    conn_status status;
    connection* next;
    connection* prev;
    event_data* r_data;
    event_data* w_data;
    int fd;
    proc_status (*proc)(connection* conn);
    void* data; // not free data
    wthread* wthrd;
};

struct conn_list {
    connection* first;
    connection* last;
};

struct wthread {
    conn_list* active;
    conn_list* wait;
    event_loop* l;
    int active_size;
    int efd;
    int listen_socket;
    int wait_size;
    pthread_spinlock_t* lock;
};

#endif
