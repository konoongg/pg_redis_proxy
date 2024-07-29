#pragma once

#include "req_result.h"
#include "../logger/logger.h"

#define UPDATE_REQV_SIZE 21 // size update reqv without key, value, table_name
#define DELETE_REQV_SIZE 30// size delete reqv without key, table_name
#define BEGIN_REQV_SIZE 6
#define COMMIT_REQV_SIZE 7
#define TRANSACTION_REQV_SIZE (BEGIN_REQV_SIZE + COMMIT_REQV_SIZE) //size BEGIN; and COMMIT;

req_result get_value(char* table, char* key, char** value, int* length);
req_result set_value(char* table, char* key, char* value);
req_result del_value(char* table, char* key);
req_result create_table(char* new_table_name);
req_result get_table_name(char*** column_names, int* n_rows);
int init_work_with_db(void);
void finish_work_with_db(void);
void finish_work_with_db_abnormally (void);
req_result do_transaction(void);
int add_op_in_transaction(operation_name op_name, int param_size, char* key, char* value, char* table_name);
int add_set(int param_size, char* key, char* value, char* table_name);
int add_del(int param_size, char* key, char* table_name);
int  init_transaction(size_t transaction_size);
void free_transaction(void);
