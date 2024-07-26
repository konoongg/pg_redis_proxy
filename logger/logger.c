#include <stdlib.h>
#include <string.h>
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"

#include "logger.h"

logger* logger_op = NULL;

//start work with logger
int init_logger(void){
    logger_op = (logger*)malloc(sizeof(logger));
    if(logger_op == NULL){
        ereport(ERROR, errmsg("can't malloc"));
        return -1;
    }
    logger_op->cur_op = NULL;
    logger_op->first_op = NULL;
    return 0;
}

// create new log about new operation
int add_log(operation_name op_name, char* key, char* value){
    if(logger_op == NULL){
        return -1;
    }
    if(logger_op->cur_op == NULL){
        logger_op->cur_op = (operation*)malloc(sizeof(operation));
        if(logger_op->cur_op == NULL){
            ereport(ERROR, errmsg("can't malloc"));
            return -1;
        }
        if(logger_op->first_op == NULL){
            logger_op->first_op = logger_op->cur_op;
        }
        logger_op->cur_op->next_op = NULL;
    }
    logger_op->cur_op->op_name = op_name;
    memcpy(logger_op->cur_op->key, key, strlen(key));
    if(value != NULL){
        memcpy(logger_op->cur_op->value, value, strlen(value));
    }
    logger_op->cur_op = logger_op->cur_op->next_op;
    return 0;
}

void clear_log(void){
    logger_op->cur_op = logger_op->first_op;
}

//finish work with logger
void free_log(void){
    operation* cur_free = logger_op->first_op;
    while(cur_free != NULL){
        operation* new_cur_free = cur_free->next_op;
        free(cur_free);
        cur_free = new_cur_free;
    }
    free(logger_op);
}
