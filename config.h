#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdint.h>

typedef enum cache_mode cache_mode;
typedef enum maxmemory_policy maxmemory_policy;
typedef struct cache_conf cache_conf;
typedef struct config_redis config_redis;
typedef struct db_conn_conf db_conn_conf;
typedef struct init_worker_conf init_worker_conf;
typedef struct proto_conf proto_conf;

void init_config(void);

struct init_worker_conf {
    int backlog_size;
    int buffer_size;
    int count_worker;
    int listen_port;
};

enum maxmemory_policy {
    noeviction,
};

enum cache_mode {
    NO_SYNC,
    ALL_SYNC,
};

struct cache_conf {
    int count_basket;
    int databases;
    int ttl_s;
    maxmemory_policy mm_policy;
    uint8_t seed;
};

struct db_conn_conf {
    int count_backend;
    char* dbname;
    char* user;
};

struct proto_conf {
    char delim;
};

struct config_redis {
    init_worker_conf worker_conf;
    cache_conf c_conf;
    db_conn_conf db_conf;
    proto_conf p_conf;
};

#endif

