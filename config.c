#include <string.h>

#include "alloc.h"
#include "config.h"

void defalt_setting_init(void);

extern config_redis config;

void defalt_setting_init(void) {
    config.c_conf.count_basket = 100000;
    config.c_conf.databases = 16;
    config.c_conf.mm_policy = noeviction;
    config.c_conf.mode = NO_SYNC;

    config.worker_conf.backlog_size = 512;
    config.worker_conf.buffer_size = 512;
    config.worker_conf.count_worker = 1;
    config.worker_conf.listen_port = 6379;

    config.db_conf.count_conneton = 4;
    config.db_conf.dbname = wcalloc(9 * sizeof(char));
    config.db_conf.user = wcalloc(9 * sizeof(char));
    memcpy(config.db_conf.dbname, "postgres", 9);
    memcpy(config.db_conf.user, "postgres", 9);

    config.p_conf.delim = '.';
}

// Initialize the config value from the corresponding file.
// If the file does not exist, set the default value for all config parameters.
void init_config(void) {
    defalt_setting_init();
}
