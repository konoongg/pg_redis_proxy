#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "config.h"
#include "error_mes.h"
#include "hash.h"
#include "strings.h"

cache_data* find_data_in_basket(cache_basket* basket, char* key, int key_size);
void free_storage(kv_storage storage);

cache* c;

int init_cache(cache_conf* conf) {
    kv_storage* storage;
    int count_basket = conf->count_basket;

    c = wcalloc(sizeof(cache));
    c->storage = wcalloc(sizeof(kv_storage));
    storage = c->storage;

    c->count_basket = count_basket;

    storage->hash_func = murmur_hash_2;
    storage->kv = wcalloc(count_basket * sizeof(cache_basket));
    memset(storage->kv, 0, count_basket * sizeof(cache_basket));

    for (int i = 0; i < count_basket; ++i) {
        int err;
        storage->kv[i].lock = wcalloc(sizeof(pthread_spinlock_t));
        err = pthread_spin_init(storage->kv[i].lock, PTHREAD_PROCESS_PRIVATE);
        if (err != 0) {
            return -1;
        }
    }

    return 0;
}

cache_basket* get_basket(char* key, int key_size) {
    kv_storage* storage = c->storage;
    u_int64_t hash = storage->hash_func(key, key_size, c->count_basket, NULL);
    return &(storage->kv[hash]);
}

cache_data* find_data_in_basket(cache_basket* basket, char* key, int key_size) {
    cache_data* data = basket->first;
    while (data != NULL) {
        if (memcmp(data->key, key, key_size) == 0 &&
            data->key_size == key_size) {
            return data;
        }
        data = data->next;
    }
    return NULL;
}

cache_get_result get_cache(cache_data new_data) {
    cache_basket* basket = get_basket(new_data.key, new_data.key_size);
    cache_get_result get_result;

    cache_data* data = find_data_in_basket(basket, new_data.key, new_data.key_size);
    if (data != NULL) {
        if (data->d_type != new_data.d_type) {
            get_result.err = true;
            get_result.err_mes = wrong_type;
        } else {
            get_result.result = data->data;
            get_result.err = false;
        }
        return get_result;
    }

    get_result.result = NULL;
    get_result.err = false;
    return get_result;
}

int set_cache(cache_data new_data) {
    cache_basket* basket = get_basket(new_data.key, new_data.key_size);

    cache_data* data = basket->first;
    while (data != NULL) {
        if (memcmp(data->key, new_data.key, new_data.key_size) == 0 && data->key_size == new_data.key_size) {
            if (data->d_type != new_data.d_type) {
                return -1;
            }
            break;
        }
        data = data->next;
    }

    if (data == NULL) {
        data = wcalloc(sizeof(cache_data));
        memset(data, 0, sizeof(cache_data));

        if (basket->first == NULL) {
            basket->first = basket->last = data;
        } else {
            basket->last->next = data;
            basket->last = basket->last->next;
        }

        data->key = wcalloc(new_data.key_size * sizeof(char*));
        data->next = NULL;
        memcpy(data->key, new_data.key, new_data.key_size);
        data->key_size = new_data.key_size;
        data->free_data = new_data.free_data;
    }

    data->last_time = time(NULL);
    if (data->last_time == -1) {
        return -1;
    }

    if (data->data != NULL) {
        data->free_data(data->data);
    }

    data->data = new_data.data;

    return 0;
}

int delete_cache(char* key, int key_size) {
    cache_basket* basket = get_basket(key, key_size);

    cache_data* data = basket->first;
    cache_data* prev_data = NULL;
    while (data != NULL) {
        if (memcmp(data->key, key, key_size) == 0 &&
            data->key_size == key_size) {
            break;
        }
        prev_data = data;
        data = data->next;
    }

    if (data == NULL) {
        return 0;
    } else if (data == basket->first) {
        basket->first = data->next;
    } else {
        prev_data->next = data->next;
    }
    data->free_data(data->data);
    free(data);
    return 1;
}

void free_cache(void) {
    for (int i = 0; i < c->count_basket; ++i) {
        cache_basket* basket = &(c->storage->kv[i]);
        cache_data* cur_data = basket->first;
        while (cur_data != NULL) {
            cache_data* new_data = cur_data->next;
            free_cache_data(cur_data->key, cur_data->key_size);
            cur_data = new_data;
        }
    }
    free(c->storage->kv);
    free(c->storage);
    free(c);
}

int lock_cache_basket(char* key, int key_size) {
    cache_basket* basket = get_basket(key, key_size);

    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        return -1;
    }

    return 0;
}

int unlock_cache_basket(char* key, int key_size) {
    cache_basket* basket = get_basket(key, key_size);

    int err = pthread_spin_unlock(basket->lock);
    if (err != 0) {
        return -1;
    }
    return 0;
}

cache_data create_data(char* key, int key_size, void* data, data_type d_type, void (*free_data)(void* data)) {
    cache_data new_data;
    memset(&new_data, 0, sizeof(new_data));
    new_data.key_size = key_size;
    new_data.key = key;
    new_data.d_type = d_type;
    new_data.free_data = free_data;
    new_data.data = data;
    return new_data;
}
