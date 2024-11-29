#include "config.h"

void defalt_setting_init(config_proxy* config);

void defalt_setting_init(config_proxy* config) {
    config->port = 6379;
    config->backlog_size = 511;
}

// Initialize the config value from the corresponding file.
// If the file does not exist, set the default value for all config parameters.
void init_config(config_proxy* config) {
    defalt_setting_init(config);
}