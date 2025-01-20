#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint64_t murmur_hash_2(char* key, int len, int count_basket, void* argv) {
    const unsigned int m = 0x5bd1e995;
    const unsigned int seed = 0;
    const int r = 24;
    unsigned int k = 0;

    unsigned int h = seed ^ len;
    const unsigned char * data = (const unsigned char *)key;

    while (len >= 4) {
        k = data[0];
        printf("k(%u) = data[0]; \n", k);
        k |= data[1];
        printf("k(%u) = data[1]; \n", k);
        k |= data[2];
        printf("k(%u) = data[2]; \n", k);
        k |= data[3];
        printf("k(%u) = data[3]; \n", k);

        printf("data[0] %d %c data[1] %d %c data[2] %d %c data[3] %d %c\n", data[0], data[0], data[1], data[1], 
                    data[2], data[2], data[3], data[3] );

        k *= m;

        printf("k(%u) *= m; \n", k);
        k ^= k >> r;
        printf("k(%u) ^= k >> r; \n", k);
        k *= m;
        printf("k(%u) *= m; \n", k);
        
        h *= m;
        h ^=k;
        printf("new h: %u new k: %u \n", h, k);
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
    printf("key: %s hash: %d \n", key, h);
    //ereport(INFO, errmsg("murmur_hash_2: key %s hash %d", key, h));
    return h;
}

int main (int argc, char** argv) {
    murmur_hash_2(argv[1], 15, 100000, NULL);
    return 0;
}