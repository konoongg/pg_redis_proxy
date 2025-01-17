#include "config.h"

void defalt_setting_init(config_redis* config);

void defalt_setting_init(config_redis* config) {
    config->c_conf.count_basket = 100000;
    config->c_conf.databases = 16;
    config->c_conf.mm_policy = noeviction;
    config->worker_conf.backlog_size = 512;
    config->worker_conf.buffer_size = 512;
    config->worker_conf.count_worker = 1;
    config->worker_conf.listen_port = 6379;
}

// Initialize the config value from the corresponding file.
// If the file does not exist, set the default value for all config parameters.
void init_config(config_redis* config) {
    defalt_setting_init(config);
}
