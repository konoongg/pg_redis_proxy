#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>

#include "redis_reqv_converter.h"
#include "../work_with_db/work_with_db.h"
#include "../configure_proxy/configure_proxy.h"
#include "../send_req_postgres/send_req_postgres.h"

/*
 * Processing part (Redis command to PostgreSQL query)
 * ["get", "key", "value"] =>
 * "SELECT value FROM pg_redis_table WHERE key=key" + return expected RESP result
 */


// get orginally receives one arg, its char* key. pg_answer stores response from postgres 
int process_get(char* key, char** pg_answer, int* size_pg_answer){
    char* value;
    int length_value = 0;
    req_result res = req_get(key, &value, &length_value);
    ereport(INFO, errmsg("START GET"));
    if(res == NON){
        ereport(LOG, errmsg("IN process_get: GET non key: %s",key));
        *pg_answer = (char*)malloc( sizeof(char));
        if(*pg_answer == NULL){
            ereport(ERROR, errmsg("ERROR MALLOC"));
            return -1;
        }
        (*pg_answer)[0] = 3;
        *size_pg_answer = 1;
    }
    else if(res == ERR_REQ){
        ereport(ERROR, errmsg("IN process_get: GET err with key: %s", key));
        return -1;
    }
    else{
        ereport(INFO, errmsg("IN process_get: GET  key: %s value: %s", key, value));
        *size_pg_answer = (length_value + 1);
        *pg_answer = (char*)malloc(  *size_pg_answer * sizeof(char));
        if(*pg_answer == NULL){
            ereport(ERROR, errmsg("can't malloc process_get: one pg_answer"));
            return -1;
        }
        (*pg_answer)[0] = 3;
        memcpy((*pg_answer) + 1, value, length_value);
        ereport(LOG, errmsg("pg_answer: %s value: %s size_pg_answer: %d ", *pg_answer, value, *size_pg_answer));
    }
    ereport(INFO, errmsg("FINISH GET"));
    return 0;
}

// should return +OK (or smth like that) if it worked
int process_set(char* key, char* value, char** pg_answer, int* size_pg_answer){
    req_result res = req_set(key, value);
    ereport(INFO, errmsg("START SET"));
    if(res == ERR_REQ){
        ereport(ERROR, errmsg("IN process_set: SET err with key: %s value: %s", key, value));
        return -1;
    }
    else{
        *pg_answer = (char*)malloc( 4 * sizeof(char));
        if(*pg_answer == NULL){
            ereport(ERROR, errmsg("ERROR MALLOC process_set: pg_answer"));
            return -1;
        }
        *size_pg_answer = 4;

        memcpy(*pg_answer, "\0OK\0", *size_pg_answer); // code of symbol '\0' is 0

        ereport(INFO, errmsg("IN process_set: SET key:%s value: %s", key, value));
    }
    return 0;
}

int process_del(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer) {
    int successful_deletions = 0;
    int count_write_sym;
    req_result res;
    char num[20];
    ereport (INFO, errmsg("START DEL, %d arguments", command_argc));
    // 0'th arg is "DEL", its ignored.
    for (int i = 1; i < command_argc; ++i) {
        res = req_del(command_argv[i]);
        if (res == OK) 
            successful_deletions++;

        if (res == ERR_REQ) {
            ereport(ERROR, errmsg("Error during deletion of key %s", command_argv[i]));
            return -1;
        }

        // res also can be NON, in this case increment doesn't happen
    }

    // If more than 9 values deleted, it will constantly return 9
    // couldn't make it easier
    count_write_sym = snprintf(num, 20, "%d", successful_deletions);
    // count_write_sym = sprintf(num, "%d", successful_deletions);
    if (count_write_sym < 0) {
        ereport(ERROR, errmsg( "snprintf err"));
        return -1;
    }

    *size_pg_answer =  count_write_sym + 1;
    *pg_answer = (char*)malloc(  *size_pg_answer * sizeof(char));
    if(*pg_answer == NULL){
        ereport(ERROR, errmsg("ERROR MALLOC process_del: pg_answer"));
        return -1;
    }
    (*pg_answer)[0] = 2;
    memcpy((*pg_answer) + 1, num, count_write_sym);
    ereport(INFO, errmsg("FINISH DEL"));
    return 0;
}

int process_ping(char** pg_answer, int* size_pg_answer){
    ereport(INFO, errmsg("IN process_ping"));
    *size_pg_answer = 6;
    *pg_answer = (char*)malloc(*size_pg_answer * sizeof(char));
    if(*pg_answer == NULL){
        ereport(ERROR, errmsg("ERROR MALLOC process_ping: pg_answer "));
        return -1;
    }

    memcpy(*pg_answer, "\0PONG\0", *size_pg_answer);
    return 0;
}

int process_command(int command_argc, char** command_argv) {
    ereport(INFO, errmsg("IN process_command"));
    return 0;// plug
    // or better: this should return something like "$3\r\n(all commands supported)\r\n"
}

// error sizes are different, but in general case they are not larger than 256.
int process_error(char** pg_answer, int* size_pg_answer, ErrorType type) {
    ereport(INFO, errmsg("IN process_error"));
    *pg_answer = (char*)malloc(MAX_ERROR_SIZE * sizeof(char));
    (*pg_answer)[0] = 1;

    if (type == ERROR_COMMAND_NOT_FOUND) {
        const char* const error_message = "unknown command";
        
        ereport(NOTICE, errmsg("Process command not found error"));
        *size_pg_answer = 1 + strlen(error_message);

        memcpy((*pg_answer) + 1, error_message, *size_pg_answer - 1);
        ereport(LOG, errmsg("Error message: %s, its length: %lu, pg_answer: %s, its length: %d", error_message, strlen(error_message), *pg_answer, *size_pg_answer));
    } 
    else if (type == ERROR_WRONG_ARGUMENTS_COUNT) {
        const char* const error_message = "syntax error";
        ereport(NOTICE, errmsg("Processing: wrong arguments count"));
        *size_pg_answer = 1 + strlen(error_message);

        memcpy((*pg_answer) + 1, error_message, *size_pg_answer - 1);
        ereport(LOG, errmsg("Error message: %s, its length: %lu, pg_answer: %s, its length: %d", error_message, strlen(error_message), *pg_answer, *size_pg_answer));
    }
    
    else {
        const char* const error_message = "Unknown error";

        ereport(NOTICE, errmsg("Process unknown error"));
        *size_pg_answer = 1 + strlen(error_message);

        memcpy((*pg_answer) + 1, error_message, *size_pg_answer - 1);
    }

    return 0;
}


// aAab23 => AAAB23
void to_big_case(char* string) {
    for (int i = 0; i < strlen(string); ++i) {
        if (string[i] >= 'a' && string[i] <= 'z'){
            string[i] = string[i] + ('A' - 'a');
        }
    }
}

/*
 * High-level function which redirects processing of command arguments to
 * basic cases ("get", "set", etc.)
 */
int process_redis_to_postgres(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer) {
    ereport(NOTICE, errmsg("PROCESSING STARTED %d", command_argc));
    if (command_argc == 0) {
        return -1; // nothing to process to db
    }

    to_big_case(command_argv[0]); // converting to upper, since commands are in upper case
    if (!strcmp(command_argv[0], "GET")) {
        if (command_argc < 2) {
            ereport(INFO, errmsg("Get receives 1 argument, got %d instead", command_argc - 1));
            return process_error(pg_answer, size_pg_answer, ERROR_WRONG_ARGUMENTS_COUNT);
        }
        ereport(INFO, errmsg("GET_PROCESSING"));
        return process_get(command_argv[1], pg_answer, size_pg_answer);
    }
    else if (!strcmp(command_argv[0], "SET")) {
        if (command_argc != 3) { // it must be exactly 3
            ereport(INFO, errmsg("Set receives 2 arguments, got %d instead", command_argc - 1));
            return process_error(pg_answer, size_pg_answer, ERROR_WRONG_ARGUMENTS_COUNT);
        }
        
        ereport(INFO, errmsg("SET_PROCESSING"));
        return process_set(command_argv[1], command_argv[2], pg_answer, size_pg_answer);
    } 
    else if (!strcmp(command_argv[0], "DEL")) {
        if (command_argc < 2) {
            ereport(INFO, errmsg("need at least 1 argument for DEL"));
            return process_error(pg_answer, size_pg_answer, ERROR_WRONG_ARGUMENTS_COUNT);
        }
        ereport(INFO, errmsg("DEL_PROCESSING"));

        // unlike many other commands, del receives 1+ arguments. For this reason, both command_argc and command_argv are sent.
        return process_del(command_argc, command_argv, pg_answer, size_pg_answer);
    } 
    else if (!strcmp(command_argv[0], "COMMAND")) {
        ereport(INFO, errmsg("COMMAND_PROCESSING"));
        return 0;
    }
    else if (!strcmp(command_argv[0], "PING")) {
        ereport(INFO, errmsg("PING PROCESSING"));
        return process_ping(pg_answer, size_pg_answer);
    }
    else { // command not found "exception"
        ereport(INFO, errmsg("COMMAND NOT FOUND: %s", command_argv[0]));
        return process_error(pg_answer, size_pg_answer, ERROR_COMMAND_NOT_FOUND);
    }
}
