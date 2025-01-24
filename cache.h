#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "config.h"

typedef enum data_type data_type;
typedef enum sub_reason sub_reason;
typedef struct cache cache;
typedef struct cache_basket cache_basket;
typedef struct cache_data cache_data;
typedef struct cache_get_result cache_get_result;
typedef struct kv_storage kv_storage;
typedef struct pending pending;
typedef struct pending_list pending_list;

cache_data create_data(char* key, int key_size, void* data, void (*free_data)(void* data));
int delete_cache(char* key, int key_size);
int init_cache();
int lock_cache_basket(char* key, int key_size);
int set_cache(cache_data new_data);
int subscribe(char* key, int key_size, sub_reason reason, int notify_fd);
int unlock_cache_basket(char* key, int key_size);
void free_cache(void);
void* get_cache(cache_data new_data);
void notify(char* key, int key_size, char mes);

struct pending {
    pending* next;
    int notify_fd;
};

struct pending_list {
    pending* first;
    pending* last;
};

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
    cache_get_result (*get)(cache_data new_data);
    int (*set)(cache_data new_data);
    int (*delete)(char* key, int key_size);
    kv_storage* storage;
    int count_basket;
};

enum sub_reason {
    WAIT_DATA,
    WAIT_SYNC,
};

#endif
