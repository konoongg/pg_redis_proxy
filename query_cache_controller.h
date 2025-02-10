#ifndef QUERY_CACHE_C
#define QUERY_CACHE_C

#include <pthread.h>

#include "db_data.h"
#include "event.h"

typedef enum com_reason com_reason;
typedef struct command_to_db command_to_db;
typedef struct db_worker db_worker;
typedef struct list_command list_command;

cache_data* init_cache_data(char* key, int key_size, req_table* args);
void register_command(char* key, char* req, connection* conn, com_reason reason);
void free_cache_data(cache_data* data);
void init_db_worker(void);

struct command_to_db {
    char* cmd;
    connection* conn;
    command_to_db* next;
    char* table;
    com_reason reason;
};

struct list_command {
    command_to_db* first;
    command_to_db* last;
    int count_commands = 0;
};

enum com_reason {
    CACHE_SYNC,
    CACHE_UPDATE,
};

struct db_worker {
    list_command* commands;
    backend* backends;
    int count_backends;
    wthread* wthrd;
};

#endif
