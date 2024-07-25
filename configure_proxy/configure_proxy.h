#pragma once

#include "libpq-fe.h"
#include <stdbool.h>

// #define DEFAULT_PORT            (6379) // 6379 is a default redis port
// #define DEFAULT_BACKLOG_SIZE    (512)
// #define DEFAULT_DB_COUNT        (16)

enum cashing_status{
    NO, // no cash
    GET, // only get in a cash, set and del in db
    ALWAYS // always in a cash, create dump in bd
} typedef cashing_status;

struct proxy_status{
    char cur_table_name[100];
    int cur_table_num;
    cashing_status cashing;
} typedef proxy_status;

struct ProxyConfiguration {
    unsigned int port;
    unsigned int backlog_size;
    unsigned int db_count;
} typedef ProxyConfiguration;
 // i tried to put extern here, didn't work.

int init_table(ProxyConfiguration);
bool check_table_existence(char** tables_name, char* new_table_name,  int n_rows);
char* get_cur_table_name(void);
int get_cur_table_num(void);
int get_count_table(void);
int init_proxy_status(void);
ProxyConfiguration init_configuration(void);
cashing_status get_cashing_status(void);
