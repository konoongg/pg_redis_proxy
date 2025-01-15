#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

typedef enum maxmemory_policy maxmemory_policy;
typedef struct cache_conf cache_conf;
typedef struct config_redis config_redis;
typedef struct init_worker_conf init_worker_conf;


void init_config(config_redis* config);

struct init_worker_conf {
    int backlog_size;
    int buffer_size;
    int count_worker;
    int listen_port;
};

enum maxmemory_policy {
    noeviction,
};

struct cache_conf {
    int databases;
    maxmemory_policy mm_policy;
    int count_basket;
};

struct config_redis {
    init_worker_conf worker_conf;
    cache_conf c_conf;
};

#endif

