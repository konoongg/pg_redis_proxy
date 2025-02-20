#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "config.h"
#include "storage_data.h"

typedef struct cache cache;
typedef struct cache_basket cache_basket;
typedef struct cache_get_result cache_get_result;
typedef struct data data;
typedef struct kv_storage kv_storage;

int delete_cache(char* key, int key_size);
void set_cache(cache_data* new_data);
void free_cache(void);
void init_cache(void);
value* get_cache(char* key, int key_size);

/*
* A structure describing a cache bucket.
* Locking when accessing cache data occurs at the bucket level to prevent data races
* in case two threads attempt to add data under the same key simultaneously.
* A spinlock is used for locking, as it is assumed
* that the lock will be held for a short period of time
* and that key collisions will be rare.
*/
struct cache_basket {
    cache_data* first;
    cache_data* last;
    pthread_spinlock_t* lock;
};

/*
* A structure describing the data storage.
* It contains the hash function being used,
*  which is selected during the cache initialization phase.
*/
struct kv_storage {
    cache_basket* kv;
    uint64_t (*hash_func)(void* key, int len, void* argv);
};

struct cache {
    kv_storage* storage;
    int count_basket;
};

#endif
