#pragma once

#include "libpq-fe.h"
#include <stdbool.h>

// #define DEFAULT_PORT            (6379) // 6379 is a default redis port
// #define DEFAULT_BACKLOG_SIZE    (512)
// #define DEFAULT_DB_COUNT        (16)


/*
 * Database synchronization states:
 * NO_CACHE: No cache is present.
 * GET_CACHE: Only GET requests are cached, all other requests are immediately sent to the database.
 * ONLY_CACHE: All requests go through the cache, and the cache is not synchronized with the database.
 * DEFER_DUMP: Values are initially stored in the cache, and then after a specified interval (proxy_configure->dump_time),
 * synchronization with the database is performed.
 * All modes assume that if a value is not found in the cache, the request is sent to the database.
 * */
enum dump_status{
    NO_CACHE, // no cash
    GET_CACHE, // only get in a cash, set and del in db
    ONLY_CACHE, // dont do dump in db
    DEFFER_DUMP //
} typedef dump_status;

struct proxy_status{
    char cur_table_name[100];
    int cur_table_num;
    dump_status caching;
    int dump_time; //  dump time interval in sec
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
int get_dump_time(void);
int init_proxy_status(void);
ProxyConfiguration init_configuration(void);
dump_status get_caching_status(void);
