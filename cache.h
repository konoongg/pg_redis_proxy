#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "config.h"

typedef enum data_type data_type;
typedef struct cache cache;
typedef struct cache_basket cache_basket;
typedef struct cache_data cache_data;
typedef struct cache_get_result cache_get_result;
typedef struct kv_storage kv_storage;

cache_data create_data(char* key, int key_size, void* data, data_type d_type,  void (*free_data)(void* data));
cache_get_result get_cache(cache_data new_data);
int delete_cache(char* key, int key_size);
int init_cache(cache_conf* conf);
int lock_cache_basket(char* key, int key_size);
int set_cache(cache_data new_data);
int unlock_cache_basket(char* key, int key_size);
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
    pthread_spinlock_t* lock;
};

struct kv_storage {
    cache_basket* kv;
    uint64_t (*hash_func)(char* key, int key_size, int count_basket, void* argv);
};

struct cache {
    cache_get_result (*get)(cache_data new_data);
    int (*set)(cache_data new_data);
    int (*delete)(char* key, int key_size);
    kv_storage* storage;
    int count_basket;
};

#endif
