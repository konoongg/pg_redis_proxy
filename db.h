#ifndef DB_H
#define DB_H

#define CONN_INFO_DEFAULT_SIZE 28

#include <pthread.h>

#include "libpq-fe.h"
#include "cache.h"

typedef enum backend_status backend_status;
typedef enum result_db result_db;
typedef enum set_result set_result;
typedef struct backend_connection backend_connection;
typedef struct backend_pool backend_pool;
typedef struct db_data db_data;
typedef struct req_queue req_queue;
typedef struct req_to_db req_to_db;

struct req_to_db {
    req_to_db* next;
    sub_reason reason;
    char* req;
    int size_req;
    void* data;
    char* key;
    int key_size;
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

enum result_db {
    CONN_DB_ERR = -1,
    CONN_DB_OK,
};

struct db_data {
    char*** tuples;
    int** size_value;
    int count_rows;
    int count_column;
};

#endif
