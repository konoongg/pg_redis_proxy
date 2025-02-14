#ifndef STORAGE_DATA_H
#define STORAGE_DATA_H

#include <stdbool.h>

#include "libpq-fe.h"

typedef enum db_type db_type;
typedef struct attr attr;
typedef struct cache_data cache_data;
typedef struct req_column req_column;
typedef struct req_table req_table;
typedef struct string string;
typedef struct value value;
typedef union db_data db_data;

req_table* create_req_by_pg(PGresult* res, char* table);
req_table* create_req_by_resp(char* value, int value_size);
value* create_copy_data(value* v);
void free_req(req_table* req);
void free_values(value* v);

enum db_type {
    INT,
    STRING,
};

struct string {
    char* str;
    int size;
};

union db_data {
    int num;
    string str;
};

struct attr {
    db_data* data;
    db_type type;
    char* column_name;
    bool is_nullable;
};

struct value {
    attr** values;
    int count_fields;
    int count_tuples;
};

struct cache_data {
    value* v;
    cache_data* next;
    char* key;
    int key_size;
    time_t last_time;
};

struct req_column {
    char* column_name;
    int data_size;
    char* data;
};

struct req_table {
    char* table;
    int count_fields;
    int count_tuples;
    req_column** columns;
};

#endif