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
    NULL,        // SnapshotData array of size ^^^ 
    DEFFER_DUMP // default caching regime
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
    if(proxy_configure == NULL){
        ereport(ERROR, errmsg("can't malloc proxy_configure"));
        return -1;
    }
    memcpy(proxy_configure->cur_table_name, "redis_0", 8);
    proxy_configure->cur_table_num = 0;
    // proxy_configure->caching = DEFFER_DUMP;
    proxy_configure->dump_time = 1;
    return 0;
}

int get_dump_time(void){
    return proxy_configure->dump_time;
}

char* get_cur_table_name(void){
    return proxy_configure->cur_table_name;
}

int get_cur_table_num(void){
    return proxy_configure->cur_table_num;
}

dump_status get_caching_status(void){
    return config.caching_regime;
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

// this function was created to parse integers correctly
// returns -1 on failure and 0 on success; resulting integer is stored in integer_result
int parse_int_from_value_correctly(char* parameter_value, long* integer_result) {
    long result_of_strtol;
    char* end_ptr;

    ereport(INFO, errmsg("Converting character integer to ordinary integer"));
    errno = 0;

    result_of_strtol = strtol(parameter_value, &end_ptr, 10);

    if (errno == EINVAL) {
        ereport(LOG, errmsg("Base contains unsupported value or no conversion was performed"));
        return -1;
    } 
    else if (errno == ERANGE) {
        ereport(LOG, errmsg("Resulting value is out of range"));
        return -1;
    }

    // this means that parsing hasn't started
    if (end_ptr == parameter_value) {
        ereport(LOG, errmsg("Parsing hasn't started"));
        return -1;
    }

    if (*end_ptr != '\0') {
        ereport(LOG, errmsg("Trash at the end: %s", end_ptr));
        return -1;
    }

    (*integer_result) = result_of_strtol;
    return 0;
}

/*
 * this function tries to parse redis.conf file. Correct lines in this file are parsed
 * and needed values are set.
 * 
 * Since loglevel may not be defined before execution of this function, its logged on Notice level
 */
void init_configuration(void) {
    long integer_parameter_value;
    char parameter_name[REDIS_CONFIGURATION_MAXKEY];
    char parameter_value[REDIS_CONFIGURATION_MAXVALUE];
    char line[REDIS_CONFIGURATION_MAXLINE];    
    FILE* config_file = fopen("redis.conf", "r");
    char* error_message = strerror(errno);
    int status;
    
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
            status = parse_int_from_value_correctly(parameter_value, &integer_parameter_value);
            if (status == -1) {
                ereport(NOTICE, errmsg("Couldn't parse number for port correctly"));
                continue;
            }


            config.port = integer_parameter_value;

            ereport(LOG, errmsg("Port has been changed to %ld", integer_parameter_value));
        } 
        else if (!strcmp(parameter_name, "tcp-backlog")) {
            status = parse_int_from_value_correctly(parameter_value, &integer_parameter_value);
            if (status == -1) {
                ereport(NOTICE, errmsg("Couldn't parse number for tcp-backlog correctly"));
                continue;
            }

            config.backlog_size = integer_parameter_value;
            ereport(LOG, errmsg("Backlog size has changed to %ld", integer_parameter_value));
        } 
        else if (!strcmp(parameter_name, "databases")) {
            status = parse_int_from_value_correctly(parameter_value, &integer_parameter_value);
            if (status == -1) {
                ereport(NOTICE, errmsg("Couldn't parse number for amount of databases field correctly"));
                continue;
            }

            config.db_count = integer_parameter_value;
            ereport(LOG, errmsg("Amount of db's has changed to %ld", integer_parameter_value));
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

        /*
            save in original redis.conf looks like this:
            save 3600 1 300 100
            So 2n'th number is time, and (2n+1)'th is needed amount of actions to save to disk
            (in our case, to db)
        */ 
        else if (!strcmp(parameter_name, "save")) {
            int cur_part = 0;
            char* token;
            // first time is parsed, then actions needed. In redis.conf you can
            // see how save configuration is parsed.
            bool is_cur_time = true; 
            SnapshotData* save_data = (SnapshotData*)malloc(sizeof(SnapshotData) * 16); // no more that 16 supported
            SnapshotData current_data;
            
            ereport(LOG, errmsg("Editing save periods"));

            // token is current string value, can be both saving period and actions needed
            // see default redis config (redis.conf) to find out why this way of parsing is used.
            token = strtok(parameter_value, " ");

            while (token != NULL) {
                ereport(LOG, errmsg("Parsed token %s", token));

                if (is_cur_time) {
                    status = parse_int_from_value_correctly(token, &integer_parameter_value);
                    if (status == -1) {
                        ereport(NOTICE, errmsg("Couldn't parse saving period for snapshot data correctly"));
                        continue;
                    }

                    ereport(LOG, errmsg("Putting this value as saving period: %ld", integer_parameter_value));
                    current_data.saving_period = integer_parameter_value;
                }
                else
                {
                    status = parse_int_from_value_correctly(token, &integer_parameter_value);
                    if (status == -1) {
                        ereport(NOTICE, errmsg("Couldn't parse actions needed for snapshot data correctly"));
                        continue;
                    }

                    ereport(LOG, errmsg("Putting this value as actions_needed: %ld", integer_parameter_value));
                    current_data.actions_needed = atoi(token);
                    ereport(LOG, errmsg("SnapshotData part parsed. Saving period: %d, Actions needed: %d", current_data.saving_period, current_data.actions_needed));
                    save_data[cur_part++] = current_data;
                }
                token = strtok(NULL, " ");
                is_cur_time = !is_cur_time;

            }

            config.save_data = save_data;
        }
        // parsing cache-regime
        // our program has one of 4 cache regimes: no-cache, get-cache, only-cache and deffer-dump.
        // to find more information on that, see comments in configure_proxy.h
        else if (!strcmp(parameter_name, "cache-regime")) {

            ereport(LOG, errmsg("Editing cache regime"));

            // finding out which cache regime is set:
            if (!strcmp(parameter_value, "no-cache")) {
                config.caching_regime = NO_CACHE;
                ereport(INFO, errmsg("No cache regime was selected"));
            }
            else if (!strcmp(parameter_value, "get-cache")) {
                config.caching_regime = GET_CACHE;
                ereport(INFO, errmsg("Get cache regime was selected"));
            }
            else if (!strcmp(parameter_value, "only-cache")) {
                config.caching_regime = ONLY_CACHE;
                ereport(INFO, errmsg("Only cache regime was selected"));
            }
            else if (!strcmp(parameter_value, "deffer-dump")) {
                config.caching_regime = DEFFER_DUMP;
                ereport(INFO, errmsg("Deffer dump regime was selected"));
            }
            else {
                ereport(LOG, errmsg("Wrong value for cache-regime was set: %s", parameter_value));
            }
            ereport(LOG, errmsg("Finished editing cache regime"));
        }
        else {
            ereport(INFO, errmsg("Unknown parameter parsed. Name: %s, Value: %s", parameter_name, parameter_value));
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