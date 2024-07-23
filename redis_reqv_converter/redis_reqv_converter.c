#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>

#include "redis_reqv_converter.h"
#include "../work_with_db/work_with_db.h"
#include "../configure_proxy/configure_proxy.h"

/*
 * Processing part (Redis command to PostgreSQL query)
 * ["get", "key", "value"] =>
 * "SELECT value FROM pg_redis_table WHERE key=key" + return expected RESP result
 */


// get orginally receives one arg, its char* key. pg_answer stores response from postgres 
int
process_get(char* key, char** pg_answer, int* size_pg_answer){
    char* value;
    int length_value = 0;
    req_result res = get_value(get_cur_table(), key, &value, &length_value);
    ereport(LOG, errmsg("START GET"));
    if(res == NON){
        ereport(LOG, errmsg("IN process_get: GET non key: %s",key));
        *pg_answer = (char*)malloc( sizeof(char));
        if(*pg_answer == NULL){
            ereport(LOG, errmsg("ERROR MALLOC"));
            return -1;
        }
        (*pg_answer)[0] = 3;
        *size_pg_answer = 1;
    }
    else if(res == ERR_REQ){
        ereport(LOG, errmsg("IN process_get: GET err with key: %s", key));
        return -1;
    }
    else{
        ereport(LOG, errmsg("IN process_get: GET  key: %s value: %s", key, value));
        *size_pg_answer = (length_value + 1);
        *pg_answer = (char*)malloc(  *size_pg_answer * sizeof(char));
        (*pg_answer)[0] = 3;
        memcpy((*pg_answer) + 1, value, length_value);
    }
    ereport(LOG, errmsg("FINISH GET"));
    return 0;
}

// should return +OK (or smth like that) if it worked
int
process_set(char* key, char* value, char** pg_answer, int* size_pg_answer){
    req_result res = set_value(get_cur_table(), key, value);
    ereport(LOG, errmsg("START SET"));
    if(res == ERR_REQ){
        ereport(LOG, errmsg("IN process_set: SET err with key: %s value: %s", key, value));
        return -1;
    }
    else{
        *pg_answer = (char*)malloc( 4 * sizeof(char));
        if(*pg_answer == NULL){
            ereport(LOG, errmsg("ERROR MALLOC"));
            return -1;
        }
        *size_pg_answer = 4;
        (*pg_answer)[0] = 0;
        (*pg_answer)[1] = 'O';
        (*pg_answer)[2] = 'K';
        (*pg_answer)[3] = '\0';
        ereport(LOG, errmsg("IN process_set: SET key:%s value: %s", key, value));
    }
    return 0;
}

int
process_del(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer) {
    int successful_deletions = 0;
    req_result res;

    ereport (LOG, errmsg("START DEL, %d arguments", command_argc));
    
    // 0'th arg is "DEL", its ignored.
    for (int i = 1; i < command_argc; ++i) {
        res = del_value(get_cur_table(), command_argv[i]);
        if (res == OK) 
            successful_deletions++;

        if (res == ERR_REQ) {
            ereport(ERROR, errmsg("Error during deletion of key %s", command_argv[i]));
            return -1;
        }
    }

    // If more than 9 values deleted, it will constantly return 9
    // couldn't make it easier

    if (successful_deletions > 9) {
        successful_deletions = 9;
    }

    *size_pg_answer = 3;
    (*pg_answer)[0] = 2;
    (*pg_answer)[1] = '0' + successful_deletions;
    (*pg_answer)[2] = '\0';
    ereport(LOG, errmsg("END DEL"));

    return 0;
}

int
process_command(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_command"));
    return 0;// plug
    // or better: this should return something like "$3\r\n(all commands supported)\r\n"
}

int
process_ping(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_ping")); // plug
    return 0;
    // should return smth like "+PONG" probably
}

void
to_big_case(char* string) {
    for (int i = 0; i < strlen(string); ++i) {
        if (string[i] >= 'a' && string[i] <= 'z'){
            string[i] = string[i] + ('A' - 'a');
        }
    }
}

/*
 * High-level function which redirects processing of command arguments to
 * basic cases ("get", "set", etc.)
 * TODO: all commands. Or as many commands as possible
 */
int
process_redis_to_postgres(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer) {
    ereport(LOG, errmsg("PROCESSING STARTED %d", command_argc));
    if (command_argc == 0) {
        return -1; // nothing to process to db
    }
    to_big_case(command_argv[0]); // converting to upper, since commands are in upper case
    if (!strcmp(command_argv[0], "GET")) {
        if (command_argc < 2) {
            ereport(ERROR, errmsg("need more arg for GET"));
            return -1;
        }
        ereport(LOG, errmsg("GET_PROCESSING: %s", command_argv[0]));
        return process_get(command_argv[1], pg_answer, size_pg_answer);
    }
    else if (!strcmp(command_argv[0], "SET")) {
        if (command_argc < 3) { // it must be exactly 3
            ereport(ERROR, errmsg("need more arg for SET"));
            return -1;
        }
        ereport(LOG, errmsg("SET_PROCESSING: %s", command_argv[0]));
        return  process_set(command_argv[1], command_argv[2], pg_answer, size_pg_answer);;

    } 
    else if (!strcmp(command_argv[0], "DEL")) {
        if (command_argc < 2) {
            ereport(ERROR, errmsg("need more at least 1 argument for DEL"));
            return -1;
        }
        ereport(LOG, errmsg("DEL_PROCESSING %s", command_argv[0]));
        // unlike other commands, del receives 1+ arguments. For this reason, both command_argc and command_argv are sent.
        return process_del(command_argc, command_argv, pg_answer, size_pg_answer);
    } 
    else if (!strcmp(command_argv[0], "COMMAND")) {
        ereport(LOG, errmsg("COMMAND_PROCESSING: %s", command_argv[0]));
        return 0;

    } else { // command not found "exception"
        ereport(LOG, errmsg("COMMAND NOT FOUND: %s", command_argv[0]));
        return -1;
    }
}
