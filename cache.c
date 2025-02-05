#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "config.h"
#include "hash.h"
#include "strings.h"

cache_basket* get_basket(char* key, int key_size);
cache_data* find_data_in_basket(cache_basket* basket, char* key, int key_size);
void free_storage(kv_storage storage);

cache* c;
extern config_redis config;

void free_data_from_cache(cache_data* data);

void free_data_from_cache(cache_data* data) {
    for (int i = 0; i < data->values->count_attr; ++i) {
        free(data->values->attr->data);
        free(data->values->attr);
    }

    free(data->values);
    free(data->key);
    free(data);
}

void init_cache(void) {
    kv_storage* storage;

    c = wcalloc(sizeof(cache));
    c->storage = wcalloc(sizeof(kv_storage));
    storage = c->storage;

    c->count_basket = config.c_conf.count_basket;
    storage->hash_func = murmur_hash_2;
    storage->kv = wcalloc(c->count_basket * sizeof(cache_basket));

    for (int i = 0; i < c->count_basket; ++i) {
        int err;
        (storage->kv[i]).lock = wcalloc(sizeof(pthread_spinlock_t));
        err = pthread_spin_init(storage->kv[i].lock, PTHREAD_PROCESS_PRIVATE);
        if (err != 0) {
            ereport(INFO, errmsg("init_cache: pthread_spin_lock %s", strerror(err)));
            abort();
        }
    }
}

cache_basket* get_basket(char* key, int key_size) {
    kv_storage* storage = c->storage;
    u_int64_t hash = storage->hash_func(key, key_size, c->count_basket, NULL);
    return &(storage->kv[hash]);
}

cache_data* find_data_in_basket(cache_basket* basket, char* key, int key_size) {
    cache_data* data = basket->first;
    while (data != NULL) {
        if (memcmp(data->key, key, key_size) == 0 && data->key_size == key_size) {
            return data;
        }
        data = data->next;
    }
    return NULL;
}

values* get_cache(char* key, int key_size) {
    cache_basket* basket = get_basket(key, key_size);
    cache_data* data
    values* result = NULL;

    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        ereport(INFO, errmsg("get_cache: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    data = find_data_in_basket(basket, new_data.key, new_data.key_size);
    if (data != NULL) {
        result = data->values;
    }

    err = pthread_spin_unlock(basket->lock);
    if (err != 0) {
        ereport(INFO, errmsg("get_cache: pthread_spin_unlock %s", strerror(err)));
        abort();
    }

    return result;
}

void set_cache(cache_data* new_data) {
    cache_basket* basket = get_basket(new_data->key, new_data->key_size);
    cache_data* data;

    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        ereport(INFO, errmsg("get_cache: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    data = find_data_in_basket(basket, new_data->key, new_data->key_size);
    if (data == NULL) {
        if (basket->first == NULL) {
            data = basket->first = basket->last = wcalloc(sizeof(cache_data));
        } else {
            basket->last->next = wcalloc(sizeof(cache_data));
            data = basket->last = basket->last->next
        }
        data->next = NULL;
        data->key_size = new_data->key_size;
        data->key = new_data->key;
        data->values = new_data->values;
    } else {
        for (int i = 0; i < data->count_attr; ++i) {
            free(data->values->values[i].data);
        }
        free(data->values);
        data->values = new_data->values;
    }

    data->last_time = time(NULL);
    if (data->last_time == -1) {
        ereport(INFO, errmsg("set_cache: finish  1"));
        return -1;
    }

    err = pthread_spin_unlock(basket->lock);
    if (err != 0) {
        ereport(INFO, errmsg("set_cache: pthread_spin_unlock %s", strerror(err)));
        abort();
    }

    return 0;
}

int delete_cache(char* key, int key_size) {
    cache_basket* basket = get_basket(key, key_size);

    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        ereport(INFO, errmsg("delete_cache: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    cache_data* data = basket->first;
    cache_data* prev_data = NULL;
    while (data != NULL) {
        if (memcmp(data->key, key, key_size) == 0 && data->key_size == key_size) {
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

    if (data->next == NULL) {
        basket->last = prev_data;
    }

    free_data_from_cache(data);

    err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        ereport(INFO, errmsg("delete_cache: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    return 1;
}

void free_cache(void) {
    for (int i = 0; i < c->count_basket; ++i) {
        cache_basket* basket = &(c->storage->kv[i]);
        cache_data* cur_data = basket->first;
        while (cur_data != NULL) {
            cache_data* new_data = cur_data->next;
            cur_data->free_data(cur_data->data);
            cur_data = new_data;
        }
    }
    free(c->storage->kv);
    free(c->storage);
    free(c);
}

int lock_cache_basket(char* key, int key_size) {
    cache_basket* basket = get_basket(key, key_size);


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

cache_data* create_cache_data(char* key, int key_size) {
    cache_data data = wcalloc(sizeof(cache_data));
    data.key = wcalloc(key_size * sizeof(char));
    data.key_size = key_size;
    data.values = wcalloc();
}
