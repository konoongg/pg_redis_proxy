#pragma once
#include "libpq-fe.h"

#define DEFAULT_PORT            (6379) // 6379 is a default redis port
#define DEFAULT_BACKLOG_SIZE    (512)
#define DEFAULT_DB_COUNT        (16)

struct proxy_status{
    char cur_table[100];
} typedef proxy_status;

int init_redis_listener(void);
int init_table(void);
char check_table(char* new_table_name,  PGresult* res);
char* get_cur_table(void);
int init_proxy_status(void);
