#include "postgres.h"
#include "fmgr.h"
#include "utils/rel.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "postgres_reqv_converter.h"

int
CreateSimplStr(char* reqv, char** answer, size_t size_reqv, int* size_answer){
    int count_shell_sym = 3;
    int size_reqv_data = size_reqv - 2;
    *size_answer = size_reqv_data + count_shell_sym;
    ereport(LOG, errmsg("START CreateSimplStr: %s ", reqv));
    *answer = (char*)malloc(*size_answer * sizeof(char));
    if(*answer == NULL){
        ereport(LOG, errmsg("ERROR MALLOC"));
        return -1;
    }
    (*answer)[0] = '+';
    memcpy(*answer + 1, reqv + 1, size_reqv_data);
    (*answer)[(*size_answer)- 1] = '\n';
    (*answer)[(*size_answer) - 2] = '\r';
    ereport(LOG, errmsg("RESULT CreateSimplStr : %s ", *answer));
    return 0;
}

//count_shell_sym is /n /r and size
int
CreateStr(char* reqv, char** answer, size_t size_reqv, int* size_answer){
    int count_write_sym = 0;
    int count_shell_sym = 0;
    size_t size_reqv_data = (size_reqv == 1) ? 0 : size_reqv - 2;
    char size [20]; // max count symbol for size_t
    ereport(LOG, errmsg("START CreateStr: %s %ld ", reqv, size_reqv));
    if(size_reqv_data == 0){
        count_write_sym = 2;
        size[0] = '-';
        size[1] = '1';
        count_shell_sym = 1 + count_write_sym + 2; // 1 - $; count_write_sym - size; 2 - /r/n after size;
    }
    else{
        count_write_sym = sprintf(size, "%ld", size_reqv_data);
        if (count_write_sym < 0) {
            ereport(ERROR, errmsg( "sprintf err"));
            return -1;
        }
        count_shell_sym = 1 + count_write_sym + 2 + 2;  // 1 - $; count_write_sym - size; 2 - /r/n after size; 2 = /r/n after data;
    }
    *size_answer = size_reqv_data + count_shell_sym;
    ereport(LOG, errmsg("size_reqv_data: %ld, count_shell_sym: %d *size_answer: %d %p", size_reqv_data, count_shell_sym, *size_answer, *answer));
    *answer = (char*)malloc(*size_answer * sizeof(char));
    ereport(LOG, errmsg("SUC MALLOC"));
    if(*answer == NULL){
        ereport(LOG, errmsg("ERROR MALLOC"));
        return -1;
    }
    (*answer)[0] = '$';
    memcpy(*answer + 1, size,  count_write_sym);
    ereport(LOG, errmsg("CreateStr: %d %d SIZE: %d", 1 + count_write_sym, 1 + count_write_sym + 1,*size_answer ));
    (*answer)[1 + count_write_sym] = '\r';
    (*answer)[1 + count_write_sym + 1] = '\n';
    if(size_reqv_data != 0){
        memcpy(*answer + 1 + count_write_sym + 2, reqv + 1,  size_reqv_data);
        (*answer)[(*size_answer)- 1] = '\n';
        (*answer)[(*size_answer) - 2] = '\r';
    }
    return 0;
}

/*
 * For Simple Strings the first byte of the reply is "+" (code  0)
 * For Errors the first byte of the reply is "-" (code  1)
 * For Integers the first byte of the reply is ":" (code  2)
 * For Bulk Strings the first byte of the reply is "$" (code  3)
 * For Arrays the first byte of the reply is "*" (code  4)
 * The first byte is the response code, followed by the response from Postgres
 */
int
define_type_req(char* reqv, char** answer, size_t size_reqv, int* size_answer){
    ereport(LOG, errmsg("START define_type_req: %d ", reqv[0]));
    if(reqv[0] == 0){
        return CreateSimplStr(reqv, answer,size_reqv, size_answer);
    }
    else if(reqv[0] == 1){
        //CreateErr();
    }
    else if(reqv[0] == 2){
        //CreateInt();
    }
    else if(reqv[0] == 3){
        return CreateStr(reqv, answer, size_reqv, size_answer);
    }
    else if(reqv[0] == 4){
        //CreateArray();
    }
    return -1;
}
