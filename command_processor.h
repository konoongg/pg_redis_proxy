#ifndef C_PROCESSOR_H
#define C_PROCESSOR_H

#include <stdint.h>

#include "connection.h"

#define COMMAND_DICT_SIZE 3

typedef enum process_result  process_result;
typedef struct command_dict command_dict;
typedef struct command_entry command_entry;
typedef struct entris entris;
typedef struct pg_data pg_data;
typedef struct redis_command redis_command;
typedef struct tuple_list tuple_list;

process_result process_command(client_req* req, answer* answ);
void free_resp_answ(void* ptr);
void init_commands(void);
void process_err(answer* answ, char* err);

struct redis_command {
    char* name;
    process_result (*func)(client_req* req, answer* answ, db_connect* db_conn);
};

struct command_dict {
    entris** commands;
    int (*hash_func)(char* key);
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
    NONE,
};

#endif
