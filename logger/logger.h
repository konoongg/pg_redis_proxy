#pragma once

#include "../proxy_hash/proxy_hash.h"

enum operation_name{
    SET,
    DEL
} typedef operation_name;

struct operation{
    operation_name op_name;
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    struct operation* next_op;
} typedef operation;

struct logger{
    operation* cur_op;
    operation* first_op;
} typedef logger;

int init_logger(void);
int add_log(operation_name op_name, char* key, char* value);
void clear_log(void);
void free_log(void);
