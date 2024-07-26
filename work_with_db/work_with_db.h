#pragma once

#include "req_result.h"

req_result get_value(char* table, char* key, char** value, int* length);
req_result set_value(char* table, char* key, char* value);
req_result del_value(char* table, char* key);
req_result create_table(char* new_table_name);
req_result get_table_name(char*** column_names, int* n_rows);
int init_work_with_db(void);
void finish_work_with_db(void);
req_result finish_abnormally (void);
