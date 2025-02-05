#ifndef QUERY_CACHE_C
#define QUERY_CACHE_C

#include "db_data.h"

typedef struct req_column req_column;
typedef struct req_table req_table;

cache_data* init_cache_data(char* key, int key_size, req_table* args);
void free_cache_data(cache_data* data);
void init_db_worker(void);

struct req_column {
    char* column_name;
    int data_size;
    char* data;
};

struct req_table {
    char* table;
    int count_args;
    req_column* columns;
};

#endif