#ifndef RESP_C_H
#define RESP_C_H

/* RESP
    +<string>\r\n - Simple Strings
    -<error message>\r\n - Error
    :<integer>\r\n - Integers
    $<length>\r\n<data>\r\n - Bulk Strings
    *<number of elements>\r\n<elements> - Arrays ($-1\r\n - null values)
*/

#define MAX_STR_NUM_SIZE 20

#include "connection.h"

int create_bulk_string_resp(answer* answ, char* src, int size);
int create_err_resp(answer* answ, char* src);
int create_num_resp(answer* answ, int num);
int create_simple_string_resp(answer* answ, char* src);

#endif