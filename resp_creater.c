#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "connection.h"
#include "resp_creater.h"
#include "storage_data.h"

const char crlf[] = "\r\n";

default_resp_answer def_resp;

def_resp.ok.answer = "+OK\r\n";
def_resp.ok.answer_size = 5;

def_resp.pong.answer = "+PONG\r\n";
def_resp.pong.answer_size = 7;

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

void create_array_resp(answer* answ, values* res) {
    int index = 0;
    char str_size_array[MAX_STR_NUM_SIZE];
    snprintf(str_size_array, MAX_STR_NUM_SIZE, "%d", res->count_attr);

    int answ_size = strlen(str_size_array) + 1 + 2; //*<size>\r\n
    answer* sub_answers = wcalloc(res->count_attr * sizeof(sub_answers));

    for (int i = 0; i < res->count_attr; ++i) {
        attr* a = &(res->attr[i]);
        switch (a->type) {
            case INT:
                create_num_resp(&sub_answers[i], a->data->num);
                break;
            case STRING:
                create_bulk_string_resp(&sub_answers[i], a->data->str.str, a->data->str.size);
                break;
        }
        answ_size +=sub_answers[i].answer_size;
    }
    answ->answer_size = answ_size;
    answ->answer = wcalloc(answ_size * sizeof(char));

    answ->answer[0] = '*';
    index++;

    memcpy(answ->answer + index, str_size_array, strlen(str_size_array));
    index += strlen(str_size_array);
    memcpy(answ->answer + index, crlf, 2);
    index += 2;

    for (int i = 0; i < res->count_attr; ++i) {
        memcpy(answ->answer + index, sub_answers[i].answer, sub_answers[i].answer_size);
        index += sub_answers[i].answer_size;
        free(sub_answers[i].answer);
    }
    free(sub_answers);
}
