#ifndef C_PROCESSOR_H
#define C_PROCESSOR_H

#include <stdint.h>

#include "connection.h"

#define COMMAND_DICT_SIZE 3

typedef enum process_result  process_result;
typedef struct command_dict command_dict;
typedef struct command_entry command_entry;
typedef struct entris entris;
typedef struct redis_command redis_command;

process_result process_command(client_req* req, answer* answ);

struct redis_command {
    char* name;
    int (*func)(client_req* req, answer* answ, db_connect* db_conn);
};

struct command_dict {
    entris** commands;
    process_result (*hash_func)(char* key);
};

struct command_entry {
    redis_command* command;
    command_entry* next;
};

struct entris {
    command_entry* first;
    command_entry* last;
};

enum process_result {
    DONE,
    PROCESS_ERR,
    DB_REQ,
};

#endif
