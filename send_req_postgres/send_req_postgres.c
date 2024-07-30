#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <stdbool.h>
#include <string.h>

#include "send_req_postgres.h"
#include "../work_with_db/req_result.h"
#include "../work_with_db/work_with_db.h"
#include "../configure_proxy/configure_proxy.h"
#include "../proxy_hash/proxy_hash.h"
#include "../logger/logger.h"

/*
 * Accepts a request to retrieve an element and, depending on the caching mode,
 * interacts with either the cache or the database.
 */
req_result req_get(char* key, char** value, int* length){
    int table_num;
    bool found;
    req_result res;
    //If the request is too large to fit into the cache, we send it directly to the database.
    bool fit_in_cache = strlen(key) + 1 < KEY_SIZE && strlen(*value) + 1 < VALUE_SIZE;
    if((get_caching_status() == GET_CACHE || get_caching_status() == ONLY_CACHE || get_caching_status() == DEFFER_DUMP) && fit_in_cache){
        table_num = get_cur_table_num();
        (*value) = check_hash_table(table_num, key, &found);
        //ereport(DEBUG1, (errmsg("RESULT: %s found: %d", *value, found)));
        if(*value != NULL){
            //ereport(DEBUG1, (errmsg("RETURN OK")));
            *length = strlen(*value) + 1;
            return OK;
        }
        else if(found){
            //ereport(DEBUG1, (errmsg("RETURN NON")));
            return NON;
        }
        res = get_value(get_cur_table_name(), key, value, length);
        //ereport(DEBUG1, (errmsg("go to db: %s %d", *value, found)));
        //ereport(DEBUG1, (errmsg("res: %d", res)));
        if(res == NON){
            //ereport(DEBUG1, (errmsg("res is NON")));
            if(set_hash_table(table_num, key, *value, 1) == -1){
                return ERR_REQ;
            }
        }
        else if(res == OK){
            //ereport(DEBUG1, (errmsg("res is Ok")));
            if(set_hash_table(table_num, key, *value, 0) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if(get_caching_status() == NO_CACHE || !fit_in_cache){
        return get_value(get_cur_table_name(), key, value, length);
    }
    ereport(ERROR, (errmsg("undefined caching status")));
    return ERR_REQ;
}

/*
 * Accepts a request to modify an element and, depending on the caching mode,
 * interacts with either the cache or the database.
 */
req_result req_set(char* key, char* value){
    int table_num;
    req_result res;
    //If the request is too large to fit into the cache, we send it directly to the database.
    bool fit_in_cache = strlen(key) + 1 < KEY_SIZE && strlen(value) + 1 < VALUE_SIZE;
    if(get_caching_status() == GET_CACHE && fit_in_cache){
        table_num = get_cur_table_num();
        res = set_value(get_cur_table_name(), key, value);
        if(res != ERR_REQ){
            if (set_hash_table(table_num, key, value, 0) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if((get_caching_status() == ONLY_CACHE || get_caching_status() == DEFFER_DUMP) && fit_in_cache){
        table_num = get_cur_table_num();
        if (set_hash_table(table_num, key, value, 0) == -1){
            return ERR_REQ;
        }
        if(get_caching_status() == DEFFER_DUMP){
            if(add_log(SET, key, value) == -1){
                return ERR_REQ;
            }
        }
        return OK;
    }
    else if(get_caching_status() == NO_CACHE || !fit_in_cache){
        return set_value(get_cur_table_name(), key, value);
    }
    ereport(ERROR, (errmsg("undefined caching status")));
    return ERR_REQ;
}

/*
 * Accepts a request to delete an element and, depending on the caching mode,
 * interacts with either the cache or the database
 */
req_result req_del(char* key){
    int table_num;
    req_result res;
    //If the request is too large to fit into the cache, we send it directly to the database.
    bool fit_in_cache = strlen(key) + 1 < KEY_SIZE ;
    if(get_caching_status() == GET_CACHE && fit_in_cache) {
        table_num = get_cur_table_num();
        res = del_value(get_cur_table_name(), key);
        if(res != ERR_REQ){
            if (set_hash_table(table_num, key, NULL, 1) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if (( get_caching_status() == ONLY_CACHE || get_caching_status() == DEFFER_DUMP) && fit_in_cache){
        bool found = false;
        char* result = NULL;
        table_num = get_cur_table_num();
        result = check_hash_table(table_num, key, &found);
        if(result != NULL && found){
            //ereport(DEBUG1, (errmsg("DATA in cache %s", result)));
            res = OK;
        }
        else if(result == NULL && found){
            //ereport(DEBUG1, (errmsg("NULL in cache")));
            res =  NON;
        }
        else{
            //ereport(DEBUG1, (errmsg("not in cache")));
            res = del_value(get_cur_table_name(), key);
        }
        if (set_hash_table(table_num, key, NULL, 1) == -1){
            return ERR_REQ;
        }
        if(get_caching_status() == DEFFER_DUMP){
            if(add_log(DEL, key, NULL) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if(get_caching_status() == NO_CACHE || !fit_in_cache){
        return del_value(get_cur_table_name(), key);
    }
    ereport(ERROR, (errmsg("undefined caching status")));
    return ERR_REQ;
}

//The function is called either when a large number of operations have accumulated or on a timer,
// and it manages the synchronization with the database.
int sync_with_db(void){
    char* table_name = get_cur_table_name();
    size_t table_name_size = strlen(table_name);
    logger* logger_op = get_logger();
    operation* cur_op;
    ereport(DEBUG1, (errmsg("START SYNC")));
    if(logger_op == NULL){
        return -1;
    }
    cur_op = logger_op->first_op;
    if(cur_op == NULL || logger_op->sum_size_operation == 0){
        return 0;
    }
    if (init_transaction(logger_op->sum_size_operation) == -1){
        return -1;
    }
    ereport(DEBUG1, (errmsg("COUNT OPERATION: %ld", logger_op->count_operation)));
    for(int i = 0; i < logger_op->count_operation; ++i){
        size_t param_size = cur_op->value_size + cur_op->key_size + table_name_size;
        ereport(DEBUG1, (errmsg("cur_op->value_size: %ld  cur_op->key_size: %ld  table_name_size: %ld", cur_op->value_size,  cur_op->key_size, table_name_size)));
        ereport(DEBUG1, (errmsg("i: %d key: %s value: %s", i, cur_op->key, cur_op->value )));
        if(add_op_in_transaction(cur_op->op_name, param_size, cur_op->key, cur_op->value, table_name) == -1){
            return -1;
        }
        cur_op = cur_op->next_op;
    }
    if(do_transaction() == ERR_REQ){
        return -1;
    }
    free_transaction();
    if (clear_log() == -1){
        return -1;
    }
    ereport(DEBUG1, (errmsg("finish sync_with_db")));
    return 0;
}
