#ifndef CACHE_H
#define CACHE_H


#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef enum data_type data_type;
typedef struct cache_basket cache_basket;
typedef struct cache_data cache_data;
typedef struct kv_db kv_db;
typedef struct kv_storage kv_storage;

int get(int cur_db, char* key, void** value);
int init_cache(cache_conf* conf);
int set(int cur_db, char* key, void* value, data_type d_type);
void free_cache(void);

enum data_type {
    STRING,
};

struct cache_data {
    cache_data* next;
    char* key;
    data_type d_type;
    time_t last_time;
    void* data;
};

struct cache_basket {
    cache_data* first;
    cache_data* last;
    pthread_spinlock_t lock;
};

struct kv_storage {
    cache_basket* kv;
    int count_basket;
};

struct kv_db {
    int count_db;
    kv_storage* storages;
    uint64_t (*hash_func)(char* key);
};

#endif

