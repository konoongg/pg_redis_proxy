#pragma once

#include "libpq-fe.h"

// #define DEFAULT_PORT            (6379) // 6379 is a default redis port
// #define DEFAULT_BACKLOG_SIZE    (512)
// #define DEFAULT_DB_COUNT        (16)

struct proxy_status{
    char cur_table[100];
} typedef proxy_status;

struct ProxyConfiguration {
    unsigned int port;
    unsigned int backlog_size;
    unsigned int db_count;
} typedef ProxyConfiguration;
 // i tried to put extern here, didn't work.

int init_table(ProxyConfiguration);
bool check_table_existence(char** tables_name, char* new_table_name,  int n_rows);
char* get_cur_table(void);
int init_proxy_status(void);
ProxyConfiguration init_configuration(void);