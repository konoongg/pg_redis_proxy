#ifndef QUERY_CACHE_C
#define QUERY_CACHE_C

#include <pthread.h>

#include "db_data.h"
#include "event.h"

typedef enum com_reason com_reason;
typedef struct command_to_db command_to_db;
typedef struct db_worker db_worker;
typedef struct list_command list_command;
typedef struct req_column req_column;
typedef struct req_table req_table;

cache_data* init_cache_data(char* key, int key_size, req_table* args);
void free_cache_data(cache_data* data);
void init_db_worker(void);

struct command_to_db {
    char* cmd;
    int notify_fd;
    command_to_db* next;
};

struct list_command {
    command_to_db* first;
    command_to_db* last;
    int count_commands = 0;
};

struct req_column {
    char* column_name;
    int data_size;
    char* data;
};

struct req_table {
    char* table;
    int count_args;
    int count_tuple;
    req_column** columns;
};

enum com_reason {
    SYNC,
    CACHE_UPDATE,
};

struct db_worker {
    list_command* commands;
    backend* backends;
    int count_backends;
    wthread* wthrd;
    pthread_spinlock_t* lock;
};

#endif
