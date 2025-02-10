#ifndef RESP_C_H
#define RESP_C_H

/* RESP
    +<string>\r\n - Simple Strings
    -<error message>\r\n - Error
    :<integer>\r\n - Integers
    $<length>\r\n<data>\r\n - Bulk Strings
    *<number of elements>\r\n<elements> - Arrays ($-1\r\n - null values)
*/



#include "connection.h"

typedef enum resp_type resp_type;
typedef struct resp_string_arg resp_string_arg;
typedef struct resp_bulk_string_arg resp_bulk_string_arg;
typedef struct resp_array_arg resp_array_arg;
typedef struct resp_int_arg resp_int_arg;
typedef union generic_resp_arg generic_resp_arg;
typedef struct default_resp_answer default_resp_answer;

void create_array_resp(answer* answ, values* res);
void create_num_resp(answer* answ, int num);

enum resp_type {
    INT,
    STRING,
    BULK_STRING,
    ARRAY,
};

struct resp_string_arg{
    char* src;
};

struct resp_bulk_string_arg{
    char* src;
    int size;
};

struct resp_array_arg{
    int count_elem;
    resp_type* elem_types;
    generic_resp_arg* elem_arg;
};

struct resp_int_arg{
    int num;
};

union generic_resp_arg {
    resp_array_arg array_arg;
    resp_int_arg int_arg;
    resp_bulk_string_arg bulk_string_arg;
    resp_string_arg string_arg;
};

struct default_resp_answer {
    answer ok;
    answer pong;
};

#endif