#ifndef DB_H
#define DB_H

#define CONN_INFO_DEFAULT_SIZE 13

#include <pthread.h>

#include "libpq-fe.h"

typedef enum backend_status backend_status;
typedef struct backend_connection backend_connection;
typedef struct backend_pool backend_pool;
typedef struct req_to_db req_to_db;
typedef struct req_queue req_queue;

int register_command(req_to_db* new_req, db_connect* db_conn);

struct req_to_db {
    req_to_db* next;
    char* req;
    int size_req;
};

struct req_queue {
    req_to_db* first;
    req_to_db* last;
    int queue_size;
    pthread_mutex_t* lock;
    pthread_cond_t* cond_has_requests;
};

enum backend_status {
    WORK,
    FREE,
};

struct backend_connection {
    PGconn* conn;
    int fd;
    backend_status status;
};

struct backend_pool {
    backend_connection** connection;
    int count_backend;
};

#endif