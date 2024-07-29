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
    size_t key_size;
    size_t value_size;
    struct operation* next_op;
} typedef operation;

struct logger{
    operation* cur_op;
    operation* first_op;
    size_t sum_size_operation; // The total amount of memory that needs to be allocated for forming a transaction.
    size_t count_operation;
} typedef logger;

int init_logger(void);
int add_log(operation_name op_name, char* key, char* value);
int clear_log(void);
void free_log(void);
logger* get_logger(void);
