#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "libpq-fe.h"

#include "configure_proxy.h"
#include "../work_with_db/work_with_db.h"

struct proxy_status* proxy_configure;

bool
check_table(char** tables_name, char* new_table_name,  int n_rows){
    for (int i = 0; i < n_rows; i++) {
        ereport(LOG, errmsg("%d: check Table name: %s and new table name: %s", i, tables_name[i], new_table_name));
        if(strcmp(tables_name[i], new_table_name) == 0){
            return true;
        }
    }
    ereport(LOG, errmsg("new table name: %s not exist",  new_table_name));
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

int
init_table(void){
    char** tables_name;
    int n_rows;
    ereport(LOG, errmsg("start init table"));
    if(init_work_with_db() == -1){
        return -1;
    }
    if(get_table_name(&tables_name, &n_rows) == ERR_REQ){
        return -1;
    }
    for(int i = 0; i < DEFAULT_DB_COUNT; ++i){
        char new_table_name[12];
        //по-хорошему тут надо првоерять, что все символу записались
        if (sprintf(new_table_name, "redis_%d", i) < 0) {
            return -1;
        }
        if(!check_table(tables_name, new_table_name, n_rows)){
            if(create_table(new_table_name) == ERR_REQ){
                return -1;
            }
        }
    }
    free(tables_name);
    finish_work_with_db();
    return 0;
}
