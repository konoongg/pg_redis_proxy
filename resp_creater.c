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

void create_array_bulk_string_resp(answer* answ, int count_attr, char** attr, int* size_attr) {
    answer** sub_answers = wcalloc(count_attr * sizeof(answer*));
    int answer_size;
    int index = 0;
    char str_size[MAX_STR_NUM_SIZE];

    snprintf(str_size, MAX_STR_NUM_SIZE, "%d", count_attr); // *<integer>\r\n

    answer_size = 1  + strlen(str_size) + 2;
    for (int i = 0; i < count_attr; ++i) {
        sub_answers[i] = wcalloc(sizeof(answer));
        create_bulk_string_resp(sub_answers[i], attr[i], size_attr[i]);
        answer_size += sub_answers[i]->answer_size;
    }

    answ->answer = wcalloc(answer_size * sizeof(char));
    answ->answer_size = answer_size;

    answ->answer[index] = '*';
    index++;
    memcpy(answ->answer + index, str_size, strlen(str_size));
    index += strlen(str_size);
    memcpy(answ->answer + index, crlf, 2);
    index += 2;

    for (int i = 0; i < count_attr; ++i) {
        memcpy(answ->answer + index, sub_answers[i]->answer, sub_answers[i]->answer_size);
        index += sub_answers[i]->answer_size;
    }
}

void init_array_by_elem(answer* answ, int count_elem, answer* elem) {
    int answer_size;
    int index = 0;
    char str_size[MAX_STR_NUM_SIZE];

    snprintf(str_size, MAX_STR_NUM_SIZE, "%d", count_elem); // *<integer>\r\n

    answer_size = 1  + strlen(str_size) + 2 + count_elem * elem->answer_size;

    free(answ->answer);

    answ->answer_size = answer_size;
    answ->answer = wcalloc(answer_size * sizeof(char));

    answ->answer[index] = '*';
    index++;
    memcpy(answ->answer + index, str_size, strlen(str_size));
    index += strlen(str_size);
    memcpy(answ->answer + index, crlf, 2);
    index += 2;

    for (int i = 0; i < count_elem; ++i) {
        memcpy(answ->answer + index, elem->answer, elem->answer_size);
        index += elem->answer_size;
    }
}

void init_array_by_elems(answer* answ, int count_elems, answer** elems) {
    int answer_size;
    int index = 0;
    char str_size[MAX_STR_NUM_SIZE];


    snprintf(str_size, MAX_STR_NUM_SIZE, "%d", count_elems); // *<integer>\r\n

    answer_size = 1 + strlen(str_size) + 2;

    for (int i = 0; i < count_elems; ++i) {
        answer_size += elems[i]->answer_size;
    }

    answ->answer_size = answer_size;
    answ->answer = wcalloc(answer_size * sizeof(char));

    answ->answer[index] = '*';
    index++;
    memcpy(answ->answer + index, str_size, strlen(str_size));
    index += strlen(str_size);
    memcpy(answ->answer + index, crlf, 2);
    index += 2;

    for (int i = 0; i < count_elems; ++i) {
        memcpy(answ->answer + index, elems[i]->answer, elems[i]->answer_size);
        index += elems[i]->answer_size;
    }
}

int get_array_size(answer* answ) {
    char *endptr;
    int count_len_array ;

    count_len_array =  strtol(answ->answer + 1, &endptr, 10);

    if (errno != 0 || *endptr != '\r') {
        return -1;
    }

    return count_len_array;
}
