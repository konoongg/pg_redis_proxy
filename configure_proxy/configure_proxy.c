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
#include "../work_with_db/req_result.h"
#include "../work_with_db/work_with_db.h"
#include "../proxy_hash/proxy_hash.h"

struct proxy_status* proxy_configure;
const ProxyConfiguration DEFAULT_CONFIG = {
    6379,       // default port
    512,        // default tcp-backlog
    16,         // default databases count
    false,      // daemonize boolean
    NULL,       // raw_bind field. Should contain info like "127.0.0.1 ::-1"
    NULL,       // log file
    0,          // count of SnapshotData structures 
    NULL        // SnapshotData array of size ^^^ 
};

ProxyConfiguration config = DEFAULT_CONFIG;

// This function checks if new_table_name is in table_names[n_rows]
bool check_table_existence(char** table_names, char* new_table_name,  int n_rows){
    for (int i = 0; i < n_rows; i++) {
        ereport(DEBUG5, errmsg("%d: check Table name: %s and new table name: %s", i, table_names[i], new_table_name));
        if(strcmp(table_names[i], new_table_name) == 0){
            return true;
        }
    }
    ereport(DEBUG5, errmsg("new table name: %s does not exist",  new_table_name));
    return false;
}

int init_proxy_status(void){
    proxy_configure = (proxy_status*) malloc(sizeof(proxy_status));
    memcpy(proxy_configure->cur_table_name, "redis_0", 8);
    proxy_configure->cur_table_num = 0;
    return 0;
}

char* get_cur_table_name(void){
    return proxy_configure->cur_table_name;
}

int get_cur_table_num(void){
    return proxy_configure->cur_table_num;
}

// this function checks if all config.db_count tables exist, and if they
// don't, creates new ones
int init_table(void){
    char** table_names;
    int n_rows;
    ereport(DEBUG5, errmsg("start init table"));
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
        ereport(DEBUG5, errmsg("test"));
        if(create_hash_table(new_table_name, i) == -1){
            return -1;
        }
    }
    free(table_names);
    finish_work_with_db();
    return 0;
}

/*
 * this function tries to parse redis.conf file. Correct lines in this file are parsed
 * and needed values are set.
 * 
 * Since loglevel may not be defined before execution of this function, its logged on Notice level
 */
void init_configuration(void) {
    int integer_parameter_value;
    char parameter_name[64];
    char parameter_value[256];
    char line[1024];    
    FILE* config_file = fopen("redis.conf", "r");
    char* error_message = strerror(errno);
    
    config = DEFAULT_CONFIG;

    if (config_file == NULL) { // check if file was opened    
        ereport(ERROR, errmsg("Couldn't start loading configuration from file redis.conf: %s", error_message));
        return;
    }
    
    // %[^\n] means read until '\n'. So there must be \n sign at the end of file
    // on both error and end of file fscanf returns the same EOF
    while (fscanf(config_file, "%[^\n]\n", line) != EOF) {
        ereport(INFO, errmsg("Parsing config line: %s", line));
        
        // skipping empty lines
        if (strlen(line) == 0)
            continue;

        // skipping comments
        if (line[0] == '#')
            continue;

        if (sscanf(line, "%s %[^\n]", parameter_name, parameter_value) == EOF) {
            error_message = strerror(errno);
            ereport(ERROR, errmsg("Error with sscanf: %s", error_message));
        }

        ereport(INFO, errmsg("Name of parsed field: '%s', value of parsed field (raw string): '%s'", parameter_name, parameter_value));

        // identifying field names and setting them to ProxyConfiguration:
        if (!strcmp(parameter_name, "port")) {
            integer_parameter_value = atoi(parameter_value);
            config.port = integer_parameter_value;

            ereport(LOG, errmsg("Port has been changed to %d", integer_parameter_value));
        } 
        else if (!strcmp(parameter_name, "tcp-backlog")) {
            integer_parameter_value = atoi(parameter_value);
            config.backlog_size = integer_parameter_value;

            ereport(LOG, errmsg("Backlog size has changed to %d", integer_parameter_value));
        } 
        else if (!strcmp(parameter_name, "databases")) {
            integer_parameter_value = atoi(parameter_value);
            config.db_count = integer_parameter_value;

            ereport(LOG, errmsg("Amount of db's has changed to %d", integer_parameter_value));
        } 
        else if (!strcmp(parameter_name, "bind")) {
            // parameter name is stored on stack. For this reason, its needed to allocate memory.

            char* raw_bind = (char*)malloc(1024 * sizeof(char));
            strcpy(raw_bind, parameter_value);

            config.raw_bind = raw_bind;

            ereport(LOG, errmsg("Raw bind value has changed to %s", parameter_value));
        }
        else if (!strcmp(parameter_name, "logfile")) {
            char *logfile = (char*)malloc(1024 * sizeof(char));
            strcpy(logfile, parameter_value);

            config.logfile = logfile;

            ereport(LOG, errmsg("Logfile has changed to %s", logfile));
        } 
        else if (!strcmp(parameter_name, "daemonize")) {

            if (!strcmp(parameter_value, "yes"))
                config.daemonize = true;

            ereport(LOG, errmsg("Daemonize bool value was set to %d", config.daemonize));
        }
        else if (!strcmp(parameter_name, "save")) {
            int cur_part = 0;
            char* token;
            bool is_cur_time = true; // firstly time, then actions needed, then repeat
            SnapshotData* save_data = (SnapshotData*)malloc(sizeof(SnapshotData) * 16); // no more that 16 supported
            SnapshotData current_data;
            
            ereport(LOG, errmsg("Editing save periods"));

            token = strtok(parameter_value, " ");

            while (token != NULL) {
                ereport(LOG, errmsg("Parsed token %s", token));

                if (is_cur_time) {
                    ereport(LOG, errmsg("Putting this value as saving period: %d", atoi(token)));
                    current_data.saving_period = atoi(token);
                }
                else
                {
                    ereport(LOG, errmsg("Putting this value as actions_needed: %d", atoi(token)));
                    current_data.actions_needed = atoi(token);
                    ereport(LOG, errmsg("SnapshotData part parsed. Saving period: %d, Actions needed: %d", current_data.saving_period, current_data.actions_needed));
                    save_data[cur_part++] = current_data;
                }
                token = strtok(NULL, " ");
                is_cur_time = !is_cur_time;

            }

            config.save_data = save_data;
        }
        else {
            ereport(LOG, errmsg("Unknown parameter parsed. Name: %s, Value: %s", parameter_name, parameter_value));
        }
    }

    if (fclose(config_file) == EOF) {
        error_message = strerror(errno);
        ereport(ERROR, errmsg("Error on closing file redis.conf: %s", error_message));
    }

}

ProxyConfiguration get_configuration() {
    return config;
}

// frees all memory allocated by ProxyConfiguration variable
void free_configuration(void) {
    free(config.raw_bind);
    free(config.logfile);
    free(config.save_data);
}