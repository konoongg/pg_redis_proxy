#ifndef STORAGE_DATA_H
#define STORAGE_DATA_H

typedef enum db_type db_type;
typedef struct attr attr;
typedef struct cache_data cache_data;
typedef struct column column;
typedef struct table table;
typedef struct values values;
typedef union db_data db_data;
typedef struct string string;

enum db_type {
    BOOL,
    DOUBLE,
    INT,
    STRING,
    NON,
};

struct column {
    db_type type;
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
    bool is_nullable;
}

struct cache_data {
    values* values;
    cache_data* next;
    char* key;
    int key_size;
    time_t last_time;
};

#endif