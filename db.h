#ifndef DB_H
#define DB_H

#define CONN_INFO_DEFAULT_SIZE 28
#define COLUMN_INFO_SIZE 99

#include "libpq-fe.h"

#include "cache.h"
#include "storage_data.h"

typedef struct column column;
typedef struct table table;
typedef struct db_meta_data db_meta_data;
void init_db();
void init_meta_db(db_meta* meta);


struct column {
    data_type type;
    bool is_nullable;
    char* column_name;
};

struct table {
    int count_column;
    column* columns;
    char* name;
};

struct db_meta_data {
    table* tables;
    int count_tables;
};

#endif
