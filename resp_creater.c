#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "connection.h"
#include "resp_creater.h"

const char crlf[] = "\r\n";

void create_simple_string_resp(answer* answ, char* src) {
    int answ_size = 1 + strlen(src) + 2;  // +<src>\n\r
    int index = 0;

    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;
    answ->answer[index] = '+';
    index++;

    memcpy(answ->answer + index, src, strlen(src));
    index += strlen(src);
    memcpy(answ->answer + index, crlf, 2);
}

void create_bulk_string_resp(answer* answ, char* src, int size) {
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
}

void create_err_resp(answer* answ, char* src) {
    int answ_size = 1 + strlen(src) + 2;  // -<error message>\r\n - Error
    int index = 0;

    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;

    answ->answer[index] = '-';
    index++;

    memcpy(answ->answer + index, src, strlen(src));
    index += strlen(src);
    memcpy(answ->answer + index, crlf, 2);
}

void create_num_resp(answer* answ, int num) {
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
}

// идея, можно в кэше хранить все же просто строку, то есть конечный овтет, протокол респа такой, что 
// можно просто переписать начало и добавить запись в конец, тогда не придется делать алокация
// достаточно будет просто пердать харнимые типы и новый картеж и он создаст все нужное

void create_array_resp(answer* answ, int count_elem, resp_type* elem_types, generic_resp_arg* elem_arg) {
    char str_num[MAX_STR_NUM_SIZE];
    int answ_size = 0;
    int index = 0;
    answer** sub_answers = wcalloc(count_elem * sizeof(answer*));
    snprintf(str_num, MAX_STR_NUM_SIZE, "%d", count_elem);

    answ_size = 1 + strlen(str_num) + 2; // *<integer>\r\n<type1>....<type2>
    for (int i = 0; i < count_elem; ++i) {
        sub_answers[i] = wcalloc(sizeof(answer));
        if (elem_types[i] == INT) {
            resp_int_arg arg = elem_arg[i].int_arg;
            create_num_resp(sub_answers[i], arg.num);
        } else if (elem_types[i] == ARRAY) {
            resp_array_arg arg = elem_arg[i].array_arg;
            create_array_resp(sub_answers[i], arg.count_elem, arg.elem_types, arg.elem_types);
        } else if (elem_types[i] == STRING) {
            resp_string_arg arg = elem_arg[i].string_arg;
            create_simple_string_resp(sub_answers[i], arg.src);
        } else if (elem_types[i] == BULK_STRING) {
            resp_bulk_string_arg arg = elem_arg[i].bulk_string_arg;
            create_bulk_string_resp(sub_answers[i], arg.src, arg.size);
        }
        answ_size += sub_answers[i]->answer_size;
    }
    answ->answer = wcalloc(answ_size * sizeof(char));
    answ->answer_size = answ_size;

    answ->answer[index] = '*';
    index++;
    memcpy(answ->answer + index, str_num, strlen(str_num));
    index += strlen(str_num);
    memcpy(answ->answer + index, crlf, 2);
    index += 2;

    for (int i = 0; i < count_elem; ++i) {
        memcpy(answ->answer + index, sub_answers[i]->answer, sub_answers[i]->answer_size);
        index += sub_answers[i]->answer_size;
    }
}
