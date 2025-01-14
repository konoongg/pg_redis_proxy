#ifndef RESP_C_H
#define RESP_C_H

/* RESP
    +<string>\r\n - Simple Strings
    -<error message>\r\n - Error
    :<integer>\r\n - Integers
    $<length>\r\n<data>\r\n - Bulk Strings
    *<number of elements>\r\n<elements> - Arrays ($-1\r\n - null values)
*/


int create_bulk_string_resp(char** result, char* src, int size);
int create_err_resp(char** result, char* src);
int create_num_resp(char** result, int num);
int create_simple_string_resp(char** result, char* src);

#endif