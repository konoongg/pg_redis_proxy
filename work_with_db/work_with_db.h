#pragma once

enum get_value_result {
    err = -1,
    ok,
    non

} typedef get_value_result;

get_value_result get_value(char* table, char* key, char** value);
int set_value(char* table, char* key, char* value);
int init_work_with_db(void);
void finish_work_with_db(void);
