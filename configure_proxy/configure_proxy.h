#pragma once

#include "libpq-fe.h"
#include <stdbool.h>

#define REDIS_CONFIGURATION_MAXKEY (256)
#define REDIS_CONFIGURATION_MAXVALUE (256)
// there'no line longer than 100 symbols in official redis.conf, but let it be 512
#define REDIS_CONFIGURATION_MAXLINE (512)

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
    int dump_time; //  dump time interval in sec
} typedef proxy_status;


// snapshot data is a structure that stores 
// saving period is a period of time between saves
// actions needed is amount of actions needed between saving
// example: save 60 100 3600 1
// it will mean that if there was at least one change and one hour passed, then 
// information is saved from cache to PostgreSQL db (originally, from redis to SSD/HDD)
struct SnapshotData {
    unsigned int saving_period;
    unsigned int actions_needed;
} typedef SnapshotData;

struct ProxyConfiguration {
    unsigned int port;
    unsigned int backlog_size;
    unsigned int db_count;
    bool daemonize;
    char* raw_bind;
    char* logfile;
    int snapshotdata_count;
    SnapshotData* save_data;
    dump_status caching_regime;
} typedef ProxyConfiguration;

extern ProxyConfiguration config;

int init_table(void);
bool check_table_existence(char** tables_name, char* new_table_name,  int n_rows);
char* get_cur_table_name(void);
int get_cur_table_num(void);
int get_count_table(void);
int init_proxy_status(void);
int get_dump_time(void);
dump_status get_caching_status(void);
int parse_int_from_value_correctly(char* parameter_value, long* integer_result);
void init_configuration(void);
void free_configuration(void);
ProxyConfiguration get_configuration(void);
