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
typedef struct pending pending;
typedef struct pending_list pending_list;

cache_data create_data(char* key, int key_size, void* data, void (*free_data)(void* data));
int delete_cache(char* key, int key_size);
int init_cache(void);
int set_cache(cache_data new_data);
void free_cache(void);
void* get_cache(cache_data new_data);

struct 

struct cache_data {
    cache_data* next;
    char* key;
    int key_size;
    time_t last_time;
    void* data;
    void (*free_data)(void* data);
    pending_list* pend_list;
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
    kv_storage* storage;
    int count_basket;
};

#endif
