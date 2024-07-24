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
    res = get_value(get_cur_table_name(), key, value, length);
    if(res != ERR_REQ){
        if(set_hash_table(table_num, key, *value) == -1){
            return ERR_REQ;
        }
    }
    return res;
}

req_result req_set(char* key, char* value){
    int table_num;
    req_result res;
    table_num = get_cur_table_num();
    res = set_value(get_cur_table_name(), key, value);
    if(res != ERR_REQ){
        if (set_hash_table(table_num, key, value) == -1){
            return ERR_REQ;
        }
    }
    return res;
}
req_result req_del(char* key){
    int table_num;
    req_result res;
    table_num = get_cur_table_num();
    res = del_value(get_cur_table_name(), key);
    if(res != ERR_REQ){
        if (set_hash_table(table_num, key, NULL) == -1){
            return ERR_REQ;
        }
    }
    return res;
}