#define _GNU_SOURCE

#include <pthread.h>

#include "postgres.h"
#include "utils/elog.h"

#include "cache.h"
#include "config.h"

kv_db db ;

int copy_data(void* src, void** dst, data_type d_type) {
    if (d_type == STRING) {
        int data_size = strlen(src) + 1;
        *dst = malloc(data_size * sizeof(char));
        if (*dst == NULL) {
            char* err_msg = strerror(errno);
            ereport(ERROR, errmsg("copy_data: malloc error - %s", err_msg));
            return -1;
        }
        memcpy(*dst, src, data_size);
    }
    return 0;
}

int init_cache(cache_conf* conf) {
    db.count_db = conf->databases;

    db.storages = malloc(conf->databases * sizeof(kv_storage));
    if (db.storages == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("init_cache: malloc error - %s", err_msg));
        return -1;
    }

    for (int i = 0; i < db.count_db; ++i ) {
        kv_storage storage = db.storages[i];
        storage.count_basket = conf->count_basket;
        storage.kv = malloc(conf->count_basket * sizeof(cache_basket));
        if (storage.kv == NULL) {
            char* err_msg = strerror(errno);
            ereport(ERROR, errmsg("init_cache: malloc error - %s", err_msg));
            for (--i; i >=0; --i) {
                free_storage(db.storages[i]);
            }
            free(db.storages);
            return -1;
        }
        memset(storage.kv, 0, conf->count_basket * sizeof(cache_basket));

        for (int j = 0; j  < storage.count_basket; ++j) {
            int err = pthread_spin_init(&(storage.kv[j].lock), PTHREAD_PROCESS_PRIVATE);
            if (err != 0) {
                ereport(ERROR, errmsg("init_cache: pthrpthread_spin_init() failed- %s\n", strerror(err)));

                for (; i >=0; --i) {
                    free_storage(db.storages[i]);
                }
                return -1;
            }
        }
    }

    return 0;
}

int get_cache(int cur_db, char* key, void** value) {
    u_int64_t hash = db.hash_func(key);
    kv_storage storage = db.storages[cur_db];
    cache_basket* basket = &(storage.kv[hash]);
    int key_size = strlen(key) + 1;

    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        ereport(ERROR, errmsg("init_cache: pthread_spin_lock() failed - %s", strerror(err)));
        return -1;
    }

    cache_data* data = basket->first;

    while (data != NULL) {
        if (strncmp(data->key, key, key_size) == 0) {
            copy_data(data->d_type, value, data->d_type);

            err = pthread_spin_unlock(basket->lock);
            if (err != 0) {
                ereport(ERROR, errmsg("init_cache: pthread_spin_lock() failed - %s", strerror(err)));
                return -1;
            }
            return 0;
        }
        data = data->next;
    }

    err = pthread_spin_unlock(basket->lock);
    if (err != 0) {
        ereport(ERROR, errmsg("init_cache: pthread_spin_lock() failed - %s", strerror(err)));
        return -1;
    }

    *value = NULL;
    return 0;
}

int set_cache(int cur_db, char* key, void* value, data_type d_type) {
    u_int64_t hash = db.hash_func(key);
    kv_storage storage = db.storages[cur_db];
    cache_basket* basket = &(storage.kv[hash]);
    int key_size = strlen(key) + 1;


    int err = pthread_spin_lock(basket->lock);
    if (err != 0) {
        ereport(ERROR, errmsg("init_cache: pthread_spin_lock() failed - %s", strerror(err)));
        return -1;
    }

    cache_data* data = basket->last;

    if (data == NULL) {
        data = malloc(sizeof(cache_data));
        if (data == NULL) {
            char* err_msg = strerror(errno);
            ereport(ERROR, errmsg("set: malloc error - %s", err_msg));

            int err = pthread_spin_unlock(basket->lock);
            if (err != 0) {
                ereport(ERROR, errmsg("set: pthread_spin_lock() failed - %s", strerror(err)));
            }

            return -1;
        }
        basket->last = basket->first = data;
    } else {
        data->next = malloc(sizeof(cache_data));
        if (data->next  == NULL) {
            char* err_msg = strerror(errno);
            ereport(ERROR, errmsg("set: malloc error - %s", err_msg));

            int err = pthread_spin_unlock(basket->lock);
            if (err != 0) {
                ereport(ERROR, errmsg("set: pthread_spin_lock() failed - %s", strerror(err)));
            }

            return -1;
        }
        basket->last = data = data->next;
    }
    data->d_type = d_type;
    strncpy(key, data->key, key_size);
    cope_data(value, &data->data, d_type);
    data->next = NULL;

    data->last_time = time(NULL);
    if (data->last_time == -1) {

        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("set: time error - %s", err_msg));

        int err = pthread_spin_unlock(basket->lock);
        if (err != 0) {
            ereport(ERROR, errmsg("set: pthread_spin_lock() failed - %s", strerror(err)));
        }
        return -1;
    }

    int err = pthread_spin_unlock(basket->lock);
    if (err != 0) {
        ereport(ERROR, errmsg("set: pthread_spin_lock() failed - %s", strerror(err)));
        return -1;
    }
    return 0;
}

void free_storage(kv_storage storage) {
    for (int i = 0; i < storage.count_basket; ++i) {
        int err = pthread_spin_destroy(&(storage.kv[i].lock));
        if (err != 0) {
            ereport(ERROR, errmsg("free_storage() failed - %s\n", strerror(err)));
        }
    }

    cache_data* cur_data = storage.kv->first;
    while(cur_data != NULL) {
        cache_data* next_data = cur_data->next;
        free(cur_data->key);
        free_data(cur_data->data, cur_data->d_type);
        free(cur_data);
        cur_data = next_data;
    }
    free(storage.kv);
}

void free_data(void* data, data_type d_type) {
    if (d_type == STRING) {
        free(data);
    }
}

void free_cache(void) {
    for (int i = 0; i < db.count_db; ++i) {
        free_storage(db.storages[i]);
    }
    free(db.storages);
}
