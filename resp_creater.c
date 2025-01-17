#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "connection.h"
#include "resp_creater.h"

int create_num(answer* answ, int num);

const char crlf[] = "\r\n";


int create_simple_string_resp(answer* answ, char* src) {
    int answ_size = 1 + strlen(src) + 2;  // +<src>\n\r
    int index = 0;

    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;
    answ->answer[index] = '+';
    index++;

    memcpy(answ->answer + index, src, strlen(src));
    index += strlen(src);
    memcpy(answ->answer + index, crlf, 2);
    return 0;
}

int create_bulk_string_resp(answer* answ, char* src, int size) {
    ereport(INFO, errmsg("create_bulk_string_resp: TEST"));
    int index = 0;
    int answ_size;
    char str_size[MAX_STR_NUM_SIZE];

    snprintf(str_size, MAX_STR_NUM_SIZE, "%d", size);

    answ_size = 1 + strlen(str_size) + 2 + size + 2;  // $<length>\r\n<data>\r\n

    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;

    answ->answer[index] = '$';
    index++;

    memcpy(answ->answer + index, str_size, strlen(str_size));
    index += strlen(str_size);
    memcpy(answ->answer + index, crlf, 2);
    index += 2;

    memcpy(answ->answer + index, src, size);
    index += size;
    memcpy(answ->answer + index, crlf, 2);

    ereport(INFO, errmsg("create_bulk_string_resp: answ->answer %s", answ->answer));
    return 0;
}

int create_err_resp(answer* answ, char* src) {
    int answ_size = 1 + strlen(src) + 2;  // -<error message>\r\n - Error
    int index = 0;

    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;

    answ->answer[index] = '-';
    index++;

    memcpy(answ->answer + index, src, strlen(src));
    index += strlen(src);
    memcpy(answ->answer + index, crlf, 2);
    return 0;
}

int create_num_resp(answer* answ, int num) {
    char str_num[MAX_STR_NUM_SIZE];
    int answ_size;
    int index = 0;

    snprintf(str_num, MAX_STR_NUM_SIZE, "%d", num);

    answ_size = 1 + strlen(str_num) + 2; // :<integer>\r\n

    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;

    answ->answer[index] = ':';
    index++;
    memcpy(answ->answer + index, str_num, strlen(str_num));
    index += strlen(str_num);
    memcpy(answ->answer + index, crlf, 2);
    return 0;
}

