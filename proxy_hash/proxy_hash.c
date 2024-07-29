#include "postgres.h"
#include "fmgr.h"
#include "utils/hsearch.h"
#include "proxy_hash.h"
#include "utils/elog.h"
#include <stdbool.h>
#include <string.h>

HTAB** hashes = NULL;
int count_hash_table = 0;

// We start working with hashes, creating a separate hash table for each database table.
int init_hashes(int count_table){
    count_hash_table = count_table;
    hashes = (HTAB**)malloc(count_table * sizeof(HTAB*));
    if(hashes == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return -1;
    }
    return 0;
}

//Create a hash table for the passed table and store it in an array of hash tables
int create_hash_table(char* table_name, int table_num){
    HASHCTL info;
    HTAB* hash_table;
    int flags = HASH_ELEM | HASH_STRINGS | HASH_CONTEXT;
    memset(&info, 0, sizeof(info));
    info.keysize = KEY_SIZE;
    info.entrysize = DATA_SIZE;
    info.hcxt = CurrentMemoryContext;
    hash_table = hash_create(table_name, 1000, &info, flags);
    if (hash_table == NULL){
        ereport(ERROR, (errmsg("Could not create hash table")));
        return -1;
    }
    hashes[table_num] = hash_table;
    return 0;
}

/*
 * We check if the element exists in the hash table; if it does, we return its value.
 * If the element is marked as deleted, we send that the element does not exist. Otherwise, we send null.
 * */
char* check_hash_table(int table_num, const char* key, bool* found){
    char* result = hash_search(hashes[table_num], key, HASH_FIND, found);
    if (result != NULL && found && result[KEY_SIZE] == 2){
        *found = true;
        return NULL;
    }
    else if(result != NULL && found){
        //ereport(DEBUG1, (errmsg("get result: %s", (char*)result)));
        if(result[KEY_SIZE] == 1){
            *found = false;
            return NULL;
        }
        return (char*)result + KEY_SIZE + 1;
    }
    else{
        //ereport(DEBUG1, (errmsg("TEST NULL")));
        *found = false;
        return NULL;
    }
}

/*
 * We address the hash table and obtain a reference to the memory area where we place the key
 * (if the key size is smaller than KEY_SIZE, we fill the remaining space with zeros),
 * then there is one byte responsible for the value state:
 * 0 - ,
 * 1 - delete
 * and after that comes the actual value.
 * if dump status = only, don't need update db
 *
 * if value == NULL, change only status, else change value and status
 */
int set_hash_table(int table_num, char* key, char* value, char new_status){
    bool found;
    char* result;
    if(key == NULL){
        ereport(ERROR, (errmsg("key can't be null")));
        return -1;
    }
    result = hash_search(hashes[table_num], key, HASH_ENTER, &found);
    if (result == NULL){
        ereport(ERROR, (errmsg("Could not insert or update element in hash table")));
        return -1;
    }
    memcpy(result, key, strlen(key + 1));
    //The key has a specific size to ensure that hashes are the same for two non-maximum-sized keys the remaining space will be filled with zeros
    if(strlen(key) + 1 < KEY_SIZE){
        memset(result + strlen(key) + 1 , 0 , KEY_SIZE - (strlen(key) + 1 ));
    }
    //ereport(DEBUG1, (errmsg("change status key: %s status: %d", key, new_status)));
    result[KEY_SIZE] = new_status;
    if(value != NULL){
        memcpy(result + KEY_SIZE + 1, value, VALUE_SIZE);
    }
    return 0;
}

// Free all hash tables
void free_hashes(void){
    for(int i = 0; i < count_hash_table; ++i){
        if (hashes[i] != NULL) {
            hash_destroy(hashes[i]);
            hashes[i] = NULL;
        }
    }
    free(hashes);
}
