#ifndef CACHE_H
#define CACHE_H


#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include "config.h"

typedef enum data_type data_type;
typedef struct cache_basket cache_basket;
typedef struct cache_data cache_data;
typedef struct kv_db kv_db;
typedef struct kv_storage kv_storage;
typedef struct cache_get_result cache_get_result;


cache_data create_data(char* key, int key_size, void* data, data_type d_type,  void (*free_data)(void* data));
cache_get_result get_cache(int cur_db, cache_data new_data);
int init_cache(cache_conf* conf);
int lock_cache_basket(int cur_db, char* key);
int set_cache(int cur_db, cache_data new_data);
int unlock_cache_basket(int cur_db, char* key);
void cache_free_key(char* key);
void free_cache(void);
enum data_type {
    STRING,
};



struct cache_data {
    cache_data* next;
    char* key;
    int key_size;
    data_type d_type;
    time_t last_time;
    void* data;
    void (*free_data)(void* data);
};

struct cache_get_result {
    void* result;
    bool err;
    char* err_mes;
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

