#include <stdint.h>
#include <string.h>

#include "postgres.h"
#include "utils/elog.h"

#include "hash.h"

int hash_pow_31_mod_100(char* key) {
    char cur_sym;
    int cur_index;
    int key_size = strlen(key);
    uint64_t  hash = 0;
    uint64_t k = 31;

    for (int i = 0; i < key_size - 2; ++i) {
        k *= 31;
    }

    cur_index = 0;
    cur_sym = key[cur_index];

    while (cur_sym != '\0') {
        hash += k * cur_sym;
        k /= 31;
        cur_index++;
        cur_sym = key[cur_index];
    }
    return hash % 100;
}

uint64_t murmur_hash_2(char* key, int len, int count_basket, void* argv) {
    const unsigned int m = 0x5bd1e995;
    const unsigned int seed = 0;
    const int r = 24;
    unsigned int k = 0;

    unsigned int h = seed ^ len;
    const unsigned char * data = (const unsigned char *)key;

    while (len >= 4) {
        k = data[0];
        k |= data[1];
        k |= data[2];
        k |= data[3];

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^=k;
        data +=4;
        len -= 4;
    }

    if (len == 3) {
        h ^= data[2] << 16;
    }
    if (len >= 2) {
        h ^= data[1] << 8;
    }
    if (len >= 1) {
        h ^= data[0];
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    h %=  count_basket;
    return h;
}
