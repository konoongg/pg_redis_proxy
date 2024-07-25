#pragma once

#include "utils/hsearch.h"
#include <stdbool.h>


#define KEY_SIZE 100
#define DATA_SIZE 200
#define VALUE_SIZE DATA_SIZE - 1 - KEY_SIZE // one byte for status

int create_hash_table(char* table_name, int table_num);
char* check_hash_table(int table_num, const char* key, bool* found);
int set_hash_table(int table_num, char* key, char* value, char new_status);
int init_hashes(int count_table);
void free_hashes(void);
