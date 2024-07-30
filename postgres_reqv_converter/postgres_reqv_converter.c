#include "postgres.h"
#include "fmgr.h"
#include "utils/rel.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "postgres_reqv_converter.h"

//Takes an input of a formatted PostgreSQL response and creates a message in the simple format for Redis.
int CreateSimplStr(char* reqv, char** answer, size_t size_reqv, int* size_answer){
    int count_shell_sym = 3;
    int size_reqv_data = size_reqv - 2;
    *size_answer = size_reqv_data + count_shell_sym;
    //ereport(DEBUG1, errmsg("START CreateSimplStr: %s ", reqv));
    *answer = (char*)malloc(*size_answer * sizeof(char));
    if(*answer == NULL){
        ereport(ERROR, errmsg("ERROR MALLOC CreateSimplStr answer"));
        return -1;
    }
    (*answer)[0] = '+';
    memcpy(*answer + 1, reqv + 1, size_reqv_data);
    (*answer)[(*size_answer)- 1] = '\n';
    (*answer)[(*size_answer) - 2] = '\r';
    //ereport(DEBUG1, errmsg("RESULT CreateSimplStr : %s ", *answer));
    return 0;
}

//count_shell_sym is /n /r and size
//Takes an input of a formatted PostgreSQL response and creates a message in the bulkString format for Redis
int CreateStr(char* reqv, char** answer, size_t size_reqv, int* size_answer){
    int count_write_sym = 0;
    int count_shell_sym = 0;
    size_t size_reqv_data = (size_reqv == 1) ? 0 : size_reqv - 2;
    char size [20]; // max count symbol for size_t
    //ereport(DEBUG1, errmsg("START CreateStr: %s %ld ", reqv, size_reqv));
    if(size_reqv_data == 0){
        count_write_sym = 2;
        size[0] = '-';
        size[1] = '1';
        count_shell_sym = 1 + count_write_sym + 2; // 1 - $; count_write_sym - size; 2 - /r/n after size;
    }
    else{
        count_write_sym = snprintf(size, 20, "%ld", size_reqv_data);
        if (count_write_sym < 0) {
            ereport(ERROR, errmsg( "snprintf err"));
            return -1;
        }
        count_shell_sym = 1 + count_write_sym + 2 + 2;  // 1 - $; count_write_sym - size; 2 - /r/n after size; 2 = /r/n after data;
    }
    *size_answer = size_reqv_data + count_shell_sym;
    //ereport(DEBUG1, errmsg("size_reqv_data: %ld, count_shell_sym: %d", size_reqv_data, count_shell_sym));
    *answer = (char*)malloc(*size_answer * sizeof(char));
    if(*answer == NULL){
        ereport(ERROR, errmsg("ERROR MALLOC CreateStr: answer"));
        return -1;
    }
    (*answer)[0] = '$';
    memcpy(*answer + 1, size,  count_write_sym);
    //ereport(DEBUG1, errmsg("CreateStr: %d %d SIZE: %d", 1 + count_write_sym, 1 + count_write_sym + 1,*size_answer ));
    (*answer)[1 + count_write_sym] = '\r';
    (*answer)[1 + count_write_sym + 1] = '\n';
    if(size_reqv_data != 0){
        memcpy(*answer + 1 + count_write_sym + 2, reqv + 1,  size_reqv_data);
        (*answer)[(*size_answer)- 1] = '\n';
        (*answer)[(*size_answer) - 2] = '\r';
    }
    //ereport(DEBUG1, errmsg("RESULT CreateStr: %s ", *answer));
    return 0;
}

//Takes an input of a formatted PostgreSQL response and creates a message in the int format for Redis.
int CreateInt (char* reqv, char** answer, size_t size_reqv, int* size_answer) {
    int count_shell_sym = 3;
    int size_reqv_data = size_reqv - 1;
    *size_answer = size_reqv_data + count_shell_sym;
    //ereport(DEBUG1, errmsg("START CreateInt: %s ", reqv));
    *answer = (char*)malloc(*size_answer * sizeof(char));
    if(*answer == NULL){
        ereport(ERROR, errmsg("ERROR MALLOC CreateInt: answer"));
        return -1;
    }
    (*answer)[0] = ':';
    memcpy(*answer + 1, reqv + 1, size_reqv_data);
    (*answer)[(*size_answer)- 1] = '\n';
    (*answer)[(*size_answer) - 2] = '\r';
    //ereport(DEBUG1, errmsg("RESULT CreateSimplStr : %s ", *answer));
    return 0;
}

/*
 * For Simple Strings the first byte of the reply is "+" (code  0)
 * For Errors the first byte of the reply is "-" (code  1)
 * For Integers the first byte of the reply is ":" (code  2)
 * For Bulk Strings the first byte of the reply is "$" (code  3)
 *  For Arrays the first byte of the reply is "*" (code  4)
 * The first byte is the response code, followed by the response from Postgres
 */
int define_type_req(char* reqv, char** answer, size_t size_reqv, int* size_answer){
    //ereport(DEBUG1, errmsg("START define_type_req: %d ", reqv[0]));
    if(reqv[0] == 0){
        return CreateSimplStr(reqv, answer,size_reqv, size_answer);
    }
    else if(reqv[0] == 1){
        //CreateErr();
    }
    else if(reqv[0] == 2){
        return CreateInt(reqv, answer, size_reqv, size_answer);
    }
    else if(reqv[0] == 3){
        return CreateStr(reqv, answer, size_reqv, size_answer);
    }
    else if(reqv[0] == 4){
        //CreateArray();
    }
    return -1;
}
