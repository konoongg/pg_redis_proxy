#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "postgres.h"
#include "utils/elog.h"

#include "resp_creater.h"

const char crlf[] = "\r\n";


int create_simple_string(char** result, char* src) {
    int result_size = 1 + strlen(src) + 2;  // +<src>\n\r
    int index = 0;

    *result = malloc(result_size * sizeof(char));
    if (*result == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("create_simple_string: malloc error - %s", err_msg));
        return -1;
    }
    *result[index] = '+';
    index++;

    memcpy(*result + index, src, strlen(src));
    index += strlen(src);
    memcpy(*result + index, crlf, 2);
    return result_size;
}

int create_bulk_string(char** result, char* src, int size) {
    int index = 0;

    char str_size[20];
    snprintf(str_size, 20, "%d", size);

    int result_size = 1 + strlen(str_size) + 2 + size + 2;  // $<length>\r\n<data>\r\n

    *result = malloc(result_size * sizeof(char));
    if (*result == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("create_bulk_string: malloc error - %s", err_msg));
        return -1;
    }

    *result[index] = '$';
    index++;

    memcpy(*result + index, str_size, strlen(str_size));
    index += strlen(str_size);
    memcpy(*result + index, crlf, 2);
    index += 2;

    memcpy(*result + index, src, size);
    index += size;
    memcpy(*result + index, crlf, 2);
    return result_size;
}

int create_err(char** result, char* src) {
    int result_size = 1 + strlen(src) + 2;  // -<error message>\r\n - Error
    int index = 0;

    *result = malloc(result_size * sizeof(char));
    if (*result == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("create_err: malloc error - %s", err_msg));
        return -1;
    }
    *result[index] = '-';
    index++;

    memcpy(*result + index, src, strlen(src));
    index += strlen(src);
    memcpy(*result + index, crlf, 2);
    return result_size;
}

int create_num(char** result, int num) {
    int index = 0;
    char str_num[20];
    snprintf(str_num, 20, "%d", num);

    int result_size = 1 + strlen(str_num) + 2; // :<integer>\r\n

    *result = malloc(result_size * sizeof(char));
    if (*result == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("create_num: malloc error - %s", err_msg));
        return -1;
    }

    *result[index] = ':';
    index++;
    memcpy(*result + index, str_num, strlen(str_num));
    index += strlen(str_num);
    memcpy(*result + index, crlf, 2);
    return result_size;
}

