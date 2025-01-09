#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

typedef struct init_worker_conf {
    int backlog_size;
    int buffer_size;
    int count_worker;
    int listen_port;
} init_worker_conf;

struct config_redis {
  init_worker_conf worker_conf;
} typedef config_redis;

void init_config(config_redis* config);

#endif

