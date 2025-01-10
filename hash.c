#include <stdint.h>
#include <string.h>

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
