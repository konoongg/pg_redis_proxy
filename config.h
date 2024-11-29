#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

struct config_proxy {
  int port;
  int backlog_size;
} typedef config_proxy;

void init_config(config_proxy* config);

#endif

