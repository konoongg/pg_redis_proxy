#pragma once

#include "req_result.h"
#include "../logger/logger.h"

#define UPDATE_REQV_SIZE 21 // size update reqv without key, value, table_name
#define DELETE_REQV_SIZE 30 // size delete reqv without key, table_name
#define CREATE_TABLE_REQV_SIZE 25 // size create table reqv without table_name
#define CREATE_HSTORE_REQV_SIZE 25 // size create hstore reqv without table_name
#define FIND_REQV_SIZE 26 // size find reqv without key and table_name
#define SELCT_REQV_SIZE 19 // size select reqv withou key and table_name
#define BEGIN_REQV_SIZE 6 // size BEGIN;
#define COMMIT_REQV_SIZE 7 //sid COMMIT;
#define TRANSACTION_REQV_SIZE (BEGIN_REQV_SIZE + COMMIT_REQV_SIZE) //size BEGIN; and COMMIT;

req_result get_value(char* table, char* key, char** value, int* length);
req_result set_value(char* table, char* key, char* value);
req_result del_value(char* table, char* key);
req_result create_table(char* new_table_name);
req_result get_table_name(char*** column_names, int* n_rows);
int init_work_with_db(void);
void finish_work_with_db(void);
req_result do_transaction(void);
int add_op_in_transaction(operation_name op_name, int param_size, char* key, char* value, char* table_name);
int add_set(int param_size, char* key, char* value, char* table_name);
int add_del(int param_size, char* key, char* table_name);
int  init_transaction(size_t transaction_size);
void free_transaction(void);
req_result finish_abnormally (void);
