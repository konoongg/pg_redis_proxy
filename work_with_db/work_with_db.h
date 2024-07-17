#pragma once

enum req_result{
    err = -1,
    ok,
    non

} typedef req_result;

req_result get_value(char* table, char* key, char** value, int* length);
req_result set_value(char* table, char* key, char* value);
int init_work_with_db(void);
void finish_work_with_db(void);
