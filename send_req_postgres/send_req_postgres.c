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

req_result req_get(char* key, char** value, int* length){
    int table_num;
    bool found;
    req_result res;
    bool fit_in_cash = strlen(key) + 1 < KEY_SIZE && strlen(*value) + 1 < VALUE_SIZE;
    if((get_cashing_status() == GET || get_cashing_status() == ALWAYS) && fit_in_cash){
        table_num = get_cur_table_num();
        (*value) = check_hash_table(table_num, key, &found);
        //ereport(LOG, (errmsg("RESULT: %s found: %d", *value, found)));
        if(*value != NULL){
            //ereport(LOG, (errmsg("RETURN OK")));
            *length = strlen(*value) + 1;
            return OK;
        }
        else if(found){
            //ereport(LOG, (errmsg("RETURN NON")));
            return NON;
        }
        //ereport(LOG, (errmsg("go to db: %s %d", *value, found)));
        res = get_value(get_cur_table_name(), key, value, length);
        if(*value == NULL){
            ereport(LOG, (errmsg("value is null")));
        }
        else{
            ereport(LOG, (errmsg("value : %p ", *value)));
        }
        if(res != ERR_REQ){
            if(set_hash_table(table_num, key, *value, 0) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if(get_cashing_status() == NO || !fit_in_cash){
        return get_value(get_cur_table_name(), key, value, length);
    }
    ereport(ERROR, (errmsg("undefined cashing status")));
    return ERR_REQ;
}

req_result req_set(char* key, char* value){
    int table_num;
    req_result res;
    bool fit_in_cash = strlen(key) + 1 < KEY_SIZE && strlen(value) + 1 < VALUE_SIZE;
    if(get_cashing_status() == GET && fit_in_cash){
        table_num = get_cur_table_num();
        res = set_value(get_cur_table_name(), key, value);
        if(res != ERR_REQ){
            if (set_hash_table(table_num, key, value, 1) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if(get_cashing_status() == ALWAYS && fit_in_cash){
        table_num = get_cur_table_num();
        if (set_hash_table(table_num, key, value, 1) == -1){
            return ERR_REQ;
        }
        return OK;
    }
    else if(get_cashing_status() == NO || !fit_in_cash){
        return set_value(get_cur_table_name(), key, value);
    }
    ereport(ERROR, (errmsg("undefined cashing status")));
    return ERR_REQ;
}

req_result req_del(char* key){
    int table_num;
    req_result res;
    bool fit_in_cash = strlen(key) + 1 < KEY_SIZE ;
    if(get_cashing_status() == GET && fit_in_cash) {
        table_num = get_cur_table_num();
        res = del_value(get_cur_table_name(), key);
        if(res != ERR_REQ){
            if (set_hash_table(table_num, key, NULL, 2) == -1){
                return ERR_REQ;
            }
        }
        return res;
    }
    else if (get_cashing_status() == ALWAYS && fit_in_cash){
        bool found = false;
        char* result = NULL;
        table_num = get_cur_table_num();
        result = check_hash_table(table_num, key, &found);
        if(result != NULL){
            ereport(LOG, (errmsg("DATA in cash %s", result)));
            res = OK;
        }
        else if(result == NULL && found){
            ereport(LOG, (errmsg("NUUL in cash")));
            res =  NON;
        }
        else{
            ereport(LOG, (errmsg("not in cash")));
            res = del_value(get_cur_table_name(), key);
        }
        if (set_hash_table(table_num, key, NULL, 2) == -1){
            return ERR_REQ;
        }
        return res;
    }
    else if(get_cashing_status() == NO || !fit_in_cash){
        return del_value(get_cur_table_name(), key);
    }
    ereport(ERROR, (errmsg("undefined cashing status")));
    return ERR_REQ;
}