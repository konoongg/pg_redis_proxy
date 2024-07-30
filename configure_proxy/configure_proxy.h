#pragma once

#include "libpq-fe.h"
#include <stdbool.h>

struct proxy_status{
    char cur_table_name[100];
    int cur_table_num;
} typedef proxy_status;

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
} typedef ProxyConfiguration;

extern ProxyConfiguration config;

int init_table(void);
bool check_table_existence(char** tables_name, char* new_table_name,  int n_rows);
char* get_cur_table_name(void);
int get_cur_table_num(void);
int get_count_table(void);
int init_proxy_status(void);
void init_configuration(void);
void free_configuration(void);
ProxyConfiguration get_configuration(void);