#ifndef STORAGE_DATA_H
#define STORAGE_DATA_H

#include "libpq-fe.h"

typedef enum db_type db_type;
typedef struct attr attr;
typedef struct cache_data cache_data;
typedef struct req_column req_column;
typedef struct req_table req_table;
typedef struct string string;
typedef struct values values;
typedef union db_data db_data;

req_table* create_req_by_pg(PGresult* res, char* table);
req_table* create_req_by_resp(char* value, int value_size);
values* create_copy_data(values* v);
void free_req(req_table* req);
void free_values(values* v);

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

struct values {
    int count_attr;
    attr* attr;
};

struct attr {
    db_data* data;
    db_type type;
    char* column_name;
    bool is_nullable;
};

struct cache_data {
    values* values;
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
    int count_field;
    int count_tuple;
    req_column** columns;
};

#endif