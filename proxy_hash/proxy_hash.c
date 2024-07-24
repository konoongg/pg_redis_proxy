#include "postgres.h"
#include "fmgr.h"
#include "utils/hsearch.h"
#include "proxy_hash.h"
#include "utils/elog.h"
#include <stdbool.h>
#include <string.h>

HTAB** hashes = NULL;
int count_hash = 0;

int init_hashes(int count_table){
    count_hash = count_table;
    hashes = (HTAB**)malloc(count_table * sizeof(HTAB*));
    if(hashes == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return -1;
    }
    return 0;
}

int create_hash_table(char* table_name, int table_num){
    HASHCTL info;
    HTAB* hash_table;
    int flags = HASH_ELEM | HASH_STRINGS;
    memset(&info, 0, sizeof(info));
    info.keysize = 500;
    info.entrysize = 500;
    hash_table = hash_create(table_name, 1000, &info, flags);
    if (hash_table == NULL){
        ereport(ERROR, (errmsg("Could not create hash table")));
        return -1;
    }
    hashes[table_num] = hash_table;
    return 0;
}

char* check_hash_table(int table_num, const char* key, bool* found){
    void* result = hash_search(hashes[table_num], key, HASH_FIND, found);
    if(found){
        //ereport(LOG, (errmsg("get result: %s", (char*)result)));
        return (char*)result;
    }
    else{
        return NULL;
    }
}

int set_hash_table(int table_num, const char* key, const char* value){
    bool found;
    void* result = hash_search(hashes[table_num], (void*)key, HASH_ENTER, &found);
    if (result == NULL){
        ereport(ERROR, (errmsg("Could not insert or update element in hash table")));
        return -1;
    }
    if(value != NULL){
        //ereport(LOG, (errmsg("set key: %s value: %s", key, value)));
        memcpy(result, value, strlen(value) + 1);
    }
    else{
        result = NULL;
    }
    return 0;
}

void free_hashes(void){
    for(int i = 0; i < count_hash; ++i){
        if (hashes[i] != NULL) {
            hash_destroy(hashes[i]);
            hashes[i] = NULL;
        }
    }
    free(hashes);
}
