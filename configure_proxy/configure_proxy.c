#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "libpq-fe.h"

#include "configure_proxy.h"
#include "../work_with_db/work_with_db.h"

struct proxy_status* proxy_configure;
const ProxyConfiguration DEFAULT_CONFIG = {6379, 512, 16};

// This function checks if new_table_name is in table_names[n_rows]
bool
check_table_existence(char** table_names, char* new_table_name,  int n_rows){
    for (int i = 0; i < n_rows; i++) {
        ereport(LOG, errmsg("%d: check Table name: %s and new table name: %s", i, table_names[i], new_table_name));
        if(strcmp(table_names[i], new_table_name) == 0){
            return true;
        }
    }
    ereport(LOG, errmsg("new table name: %s does not exist",  new_table_name));
    return false;
}

int
init_proxy_status(void){
    proxy_configure = (proxy_status*) malloc(sizeof(proxy_status));
    strcpy(proxy_configure->cur_table, "redis_0");
    return 0;
}

char*
get_cur_table(void){
    return proxy_configure->cur_table;
}


// this function checks if all config.db_count tables exist, and if they
// don't, creates new ones
int
init_table(ProxyConfiguration config){
    char** table_names;
    int n_rows;
    ereport(LOG, errmsg("start init table"));
    if(init_work_with_db() == -1){
        return -1;
    }
    if(get_table_name(&table_names, &n_rows) == ERR_REQ){
        return -1;
    }
    for(int i = 0; i < config.db_count; ++i){
        char new_table_name[12];
        //по-хорошему тут надо првоерять, что все символу записались
        if (sprintf(new_table_name, "redis_%d", i) < 0) {
            return -1;
        }
        if(!check_table_existence(table_names, new_table_name, n_rows)){
            if(create_table(new_table_name) == ERR_REQ){
                return -1;
            }
        }
    }
    free(table_names);
    finish_work_with_db();
    return 0;
}

// this function gets information from proxy_configuration.conf file
// on success returns new configuration, on failure - 1. 
ProxyConfiguration
init_configuration(void) {
    int integer_parameter_value;
    char parameter_name[50], parameter_value[50];
    FILE* config_file = fopen("redis.conf", "r");
    ProxyConfiguration proxy_config = DEFAULT_CONFIG;
    char* error_message = strerror(errno);

    if (config_file == NULL) { // check if file was opened
        ereport(ERROR, errmsg("Couldn't start loading configuration from file redis.conf: %s", error_message));
        return proxy_config;
    } 
    
    // [^=] here means "read until = sign", then = means to skip = and attract my attention in case there's no '=' in file,
    // and %[^\n] means read until '\n'. So there must be \n sign at the end of file
    // on both error and end of file fscanf returns the same EOF
    while (fscanf(config_file, "%[^ ] %[^\n]\n", parameter_name, parameter_value) != EOF) {
        ereport(LOG, errmsg("Parsing config line: key: %s, value: %s", parameter_name, parameter_value));
        integer_parameter_value = atoi(parameter_value);
        if (!strcmp(parameter_name, "port")) {
            proxy_config.port = integer_parameter_value;
            ereport(LOG, errmsg("Port has changed to %d", integer_parameter_value));

        } 
        else if (!strcmp(parameter_name, "tcp-backlog")) {
            proxy_config.backlog_size = integer_parameter_value;
            ereport(LOG, errmsg("Backlog size has changed to %d", integer_parameter_value));

        } 
        else if (!strcmp(parameter_name, "databases")) {
            proxy_config.db_count = integer_parameter_value;
            ereport(LOG, errmsg("Amount of db's has changed to %d", integer_parameter_value));
        } // on incorrect parameter name this function basically skips string
    }

    if (fclose(config_file) == EOF) {
        error_message = strerror(errno);
        ereport(ERROR, errmsg("Error on closing file redis.conf: %s", error_message));
    }

    return proxy_config;
}