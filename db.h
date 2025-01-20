#ifndef DB_H
#define DB_H

#define CONN_INFO_DEFAULT_SIZE 13

typedef struct backend_pool backend_pool;


struct backend_pool {
    PGconn** connection;
    int* connection_fd;
    int count_backend;
};

#endif DB_H