#ifndef HASH_H
#define HASH_H

#define HASH_P_31_M_100_SIZE 100

#include <stdint.h>

int hash_pow_31_mod_100(char* key);

uint64_t murmur_hash_2(char* key, int len, int count_basket, void* argv);

#endif