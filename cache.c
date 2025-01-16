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

void free_storage(kv_storage storage);
cache_data* find_data_in_basket(cache_basket* basket, char* key, int key_size);

kv_db db;

int init_cache(cache_conf* conf) {
    db.count_db = conf->databases;
    db.hash_func = murmur_hash_2;
    db.storages = wcalloc(conf->databases * sizeof(kv_storage));

    for (int i = 0; i < db.count_db; ++i) {
        kv_storage* storage = &(db.storages[i]);
        storage->count_basket = conf->count_basket;
        storage->kv = wcalloc(conf->count_basket * sizeof(cache_basket));
        memset(storage->kv, 0, conf->count_basket * sizeof(cache_basket));
        for (int j = 0; j < storage->count_basket; ++j) {
            int err;
            storage->kv[j].lock = wcalloc(sizeof(pthread_spinlock_t));
            err = pthread_spin_init(storage->kv[j].lock, PTHREAD_PROCESS_PRIVATE);
            if (err != 0) {
                //ereport(ERROR, errmsg("init_cache: pthrpthread_spin_init() failed- %s\n", strerror(err)));

                for (; i >= 0; --i) {
                    free_storage(db.storages[i]);
                }
                return -1;
            }
        }
    }

    return 0;
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

cache_get_result get_cache(int cur_db, cache_data new_data) {
    kv_storage storage = db.storages[cur_db];
    u_int64_t hash = db.hash_func(new_data.key, new_data.key_size, storage.count_basket, NULL);
    cache_basket* basket = &(storage.kv[hash]);
    cache_get_result get_result;

    cache_data* data =
        find_data_in_basket(basket, new_data.key, new_data.key_size);
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

int set_cache(int cur_db, cache_data new_data) {
    kv_storage storage = db.storages[cur_db];
    u_int64_t hash = db.hash_func(new_data.key, new_data.key_size, storage.count_basket, NULL);
    cache_basket* basket = &(storage.kv[hash]);
    cache_data* data = basket->first;

    while (data != NULL) {
        if (memcmp(data->key, new_data.key, new_data.key_size) == 0 &&
            data->key_size == new_data.key_size) {
            if (data->d_type != new_data.d_type) {
                return -1;
            }
            break;
        }
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
    }

    data->last_time = time(NULL);
    if (data->last_time == -1) {
        //char* err_msg = strerror(errno);
        //ereport(ERROR, errmsg("set: time error - %s", err_msg));
        return -1;
    }

    if (data->data != NULL) {
        data->free_data(data->data);
    }
    data->data = new_data.data;

    return 0;
}

void free_cache_key(int cur_db, char* key, int key_size) {
    kv_storage storage = db.storages[cur_db];
    u_int64_t hash = db.hash_func(key, key_size, storage.count_basket, NULL);
    cache_basket* basket = &(storage.kv[hash]);

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
        return;
    } else if (data == basket->first) {
        basket->first = data->next;
    } else {
        prev_data->next = data->next;
    }

    data->free_data(data->data);
    free(data);
}

void free_storage(kv_storage storage) {
    cache_data* cur_data = storage.kv->first;

    for (int i = 0; i < storage.count_basket; ++i) {
        int err = pthread_spin_destroy(storage.kv[i].lock);
        if (err != 0) {
            //ereport(ERROR, errmsg("free_storage() failed - %s\n", strerror(err)));
        }
    }

    while (cur_data != NULL) {
        cache_data* next_data = cur_data->next;
        free(cur_data->key);
        cur_data->free_data(cur_data->data);
        free(cur_data);
        cur_data = next_data;
    }
    free(storage.kv);
}

void free_cache(void) {
    for (int i = 0; i < db.count_db; ++i) {
        free_storage(db.storages[i]);
    }
    free(db.storages);
}

int lock_cache_basket(int cur_db, char* key, int key_size) {
    kv_storage storage = db.storages[cur_db];
    u_int64_t hash = db.hash_func(key, key_size, storage.count_basket, NULL);
    cache_basket* basket = &(storage.kv[hash]);

    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        //ereport(ERROR, errmsg("lock_cache_basket: pthread_spin_lock() failed - %s", strerror(err)));
        return -1;
    }

    return 0;
}

int unlock_cache_basket(int cur_db, char* key, int key_size) {

    kv_storage storage = db.storages[cur_db];
    u_int64_t hash = db.hash_func(key, key_size, storage.count_basket, NULL);
    cache_basket* basket = &(storage.kv[hash]);

    int err = pthread_spin_unlock(basket->lock);
    if (err != 0) {
        //ereport(ERROR, errmsg("unlock_cache_basket: pthread_spin_lock() failed - %s", strerror(err)));
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
    return new_data;
}
