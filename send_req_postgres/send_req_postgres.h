#pragma once

#include "../work_with_db/req_result.h"

req_result req_get(char* key, char** value, int* length);
req_result req_set(char* key, char* value);
req_result req_del(char* key);
int sync_with_db(void);
