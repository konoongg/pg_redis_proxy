#ifndef DB_H
#define DB_H

#define CONN_INFO_DEFAULT_SIZE 28
#define COLUMN_INFO_SIZE 99

#include "libpq-fe.h"

#include "cache.h"
#include "connection.h"
#include "storage_data.h"

typedef struct backend backend;
typedef struct column column;
typedef struct db_meta_data db_meta_data;
typedef struct table table;
typedef enum db_oper_res db_oper_res;

void init_db();
void finish_connects(backend* backends);
db_oper_res write_to_db (PGconn* conn, char* req);
column* get_column_info(char* table_name, char* column_name);

enum db_oper_res {
    READ_OPER_RES,
    WRITE_OPER_RES,
    WAIT_OPER_RES,
    ERR_OPER_RES,
};

struct backend {
    PGconn* conn;
    bool is_free;
    int fd;
    connection* conn;
};

#endif
