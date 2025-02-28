#include <stdlib.h>
#include <string.h>
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"

#include "logger.h"
#include "../work_with_db/work_with_db.h"
#include "../configure_proxy/configure_proxy.h"
#include "../send_req_postgres/send_req_postgres.h"

logger* logger_op = NULL;

//start work with logger, create logger_op
int init_logger(void){
    logger_op = (logger*)malloc(sizeof(logger));
    if(logger_op == NULL){
        ereport(ERROR, errmsg("can't malloc logger_op"));
        return -1;
    }
    logger_op->cur_op = NULL;
    logger_op->first_op = NULL;
    logger_op->sum_size_operation = 0;
    logger_op->count_operation = 0;
    return 0;
}

/*
 * Adding a new entry to the operations log:
 * If there are already created nodes, their contents are overwritten.
 * If there is no next node, it is created and becomes the current node,
 * and data is written into it. If there is no current node, it is created,
 * and data is written into it.
 * */
int add_log(operation_name op_name, char* key, char* value){
    ereport(DEBUG1, errmsg("add_log %ld op_name: %d", logger_op->count_operation, op_name));
    if(logger_op == NULL){
        ereport(ERROR, errmsg("logger not exist"));
        return -1;
    }
    if(logger_op->cur_op == NULL){
        ereport(DEBUG1, errmsg("create op"));
        logger_op->cur_op = (operation*)malloc(sizeof(operation));
        if(logger_op->cur_op == NULL){
            ereport(ERROR, errmsg("can't malloc logger_op->cur_op"));
            return -1;
        }
        if(logger_op->first_op == NULL){
            logger_op->first_op = logger_op->cur_op;
        }
        logger_op->cur_op->next_op = NULL;
    }
    // if logger_op->sum_size_operation != 0, it is not first operation
    else if (logger_op->cur_op->next_op == NULL && logger_op->sum_size_operation != 0 ){
        ereport(DEBUG1, errmsg("create next op"));
        logger_op->cur_op->next_op = (operation*)malloc(sizeof(operation));
        if(logger_op->cur_op == NULL){
            ereport(ERROR, errmsg("can't malloc logger_op->cur_op->next_op "));
            return -1;
        }
        logger_op->cur_op = logger_op->cur_op->next_op;
        logger_op->cur_op->next_op = NULL;
    }
    else if (logger_op->sum_size_operation != 0){
        ereport(DEBUG1, errmsg("logger_op->sum_size_operation is NOT  0"));
        logger_op->cur_op = logger_op->cur_op->next_op;
    }
    logger_op->cur_op->op_name = op_name;
    logger_op->cur_op->key_size = strlen(key);
    ereport(DEBUG1, errmsg("key: %s size: %ld", key, strlen(key)));
    memcpy(logger_op->cur_op->key, key, logger_op->cur_op->key_size);
    logger_op->cur_op->key[logger_op->cur_op->key_size] = '\0';
    ereport(DEBUG1, errmsg("logger_op->first_op->key: %s logger_op->cur_op->key: %s", logger_op->first_op->key, logger_op->cur_op->key));
    if(value != NULL){
        logger_op->cur_op->value_size = strlen(value);
        memcpy(logger_op->cur_op->value, value, logger_op->cur_op->value_size);
        logger_op->cur_op->value[logger_op->cur_op->value_size] = '\0';
    }
    else{
        logger_op->cur_op->value_size = 0;
    }
    ereport(DEBUG1, errmsg(" logger_op->sum_size_operation %ld", logger_op->sum_size_operation));
    logger_op->sum_size_operation += logger_op->cur_op->key_size + logger_op->cur_op->value_size;
    ereport(DEBUG1, errmsg("logger_op->cur_op->key_size: %ld logger_op->cur_op->value_size: %ld",logger_op->cur_op->key_size , logger_op->cur_op->value_size));
    logger_op->sum_size_operation += strlen(get_cur_table_name());
    if(op_name == SET){
        logger_op->sum_size_operation += UPDATE_REQV_SIZE;
        ereport(DEBUG1, errmsg("lUPDATE_REQV_SIZE %d",UPDATE_REQV_SIZE));
    }
    else if(op_name == DEL){
        logger_op->sum_size_operation += DELETE_REQV_SIZE;
        ereport(DEBUG1, errmsg("DELETE_REQV_SIZE %d",DELETE_REQV_SIZE));
    }
    logger_op->count_operation++;
    //If the number of operations exceeds the maximum, perform a forced synchronization with the database
    if(logger_op->count_operation == MAX_COUNT_OPERATION){
        if (sync_with_db() == -1){
            return -1;
        }
    }
    ereport(DEBUG1, errmsg("finish_log"));
    return 0;
}

/*
 * To avoid allocating memory each time an operation occurs,
 *  previously allocated nodes are not freed.
 *  The operations pointer is reset to the first element of the queue,
 *  and values are overwritten. New memory will only be allocated
 *  if there are more operations than in the previous instance.
 *  */
int clear_log(void){
    if(logger_op == NULL){
        ereport(ERROR, errmsg("logger not exist"));
        return -1;
    }
    logger_op->sum_size_operation = 0;
    logger_op->count_operation = 0;
    logger_op->cur_op = logger_op->first_op;
    return 0;
}

//finish work with logger
void free_log(void){
    operation* cur_free;
    if(logger_op == NULL){
        return;
    }
    cur_free = logger_op->first_op;
    while(cur_free != NULL){
        operation* new_cur_free = cur_free->next_op;
        free(cur_free);
        cur_free = new_cur_free;
    }
    free(logger_op);
}

//returns the head of the operation queue
logger* get_logger(void){
    if(logger_op == NULL){
        ereport(ERROR, errmsg("logger not exist"));
        return NULL;
    }
    return logger_op;
}
