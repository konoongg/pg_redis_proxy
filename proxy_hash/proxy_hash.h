#pragma once

#include "utils/hsearch.h"
#include <stdbool.h>

int create_hash_table(char* table_name, int table_num);
char* check_hash_table(int table_num, const char* key, bool* found);
int set_hash_table(int table_num, const char* key, const char* value);
int init_hashes(int count_table);
void free_hashes(void);
