#include <errno.h>
#include <stdbool.h>

#include "postgres.h"
#include "utils/elog.h"

#include "data_parser.h"
#include "connection.h"

#define check_crlf(cur_buffer_index, max_buffer_index) \
        cur_buffer_index++; \
        if (cur_buffer_index == max_buffer_index) { \
            ereport(ERROR, errmsg("pars_data: unknown parsing state")); \
            return ERR; \
        } \
        c = data->read_buffer[cur_buffer_index]; \
        if (c != '\n') { \
            ereport(ERROR, errmsg("pars_data: unknown parsing state")); \
            return ERR; \
        }


void replace_part_of_buffer(socket_read_data* data, int cur_buffer_index);


// saves data to a buffer if more than one packet has been written off
void replace_part_of_buffer(socket_read_data* data, int cur_buffer_index) {
    memmove(data->read_buffer, data->read_buffer + cur_buffer_index, data->cur_buffer_size - cur_buffer_index);
    data->cur_buffer_size -=  cur_buffer_index;
}

/*
 * the function works on the basis of a finite state machine,
 * it takes a character from the buffer and, depending on the state of the machine,
 * performs the necessary actions
 * The parsing state is preserved if the data arrives incomplete;
 * the received data will be processed, and the parsing state is saved. When the remaining data arrives,
 * parsing will continue correctly.
 */
Eexit_status pars_data(socket_read_data* data) {
    for (int cur_buffer_index = 0; cur_buffer_index < data->cur_buffer_size; ++cur_buffer_index) {
        char c = data->read_buffer[cur_buffer_index];
        Eread_status status = data->parsing.read_status;
        char* new_str;

        ereport(INFO, errmsg("pars_data: c %d %c", c, c));

        ereport(INFO, errmsg("pars_data: data->read_buffer %s", data->read_buffer));
        if (c == '*' && status == ARRAY_WAIT) {
        ereport(INFO, errmsg("pars_data: 1"));
            data->parsing.read_status = ARGC_WAIT;
        } else if ((c >= '0' && c <= '9')  && (status == NUM_WAIT || status == ARGC_WAIT) ) {
            ereport(INFO, errmsg("pars_data: 2"));
            data->parsing.parsing_num = (data->parsing.parsing_num * 10) + (c - '0');
        } else if (c == '\r' && status == ARGC_WAIT) {

            ereport(INFO, errmsg("pars_data: 3"));
            check_crlf(cur_buffer_index, data->cur_buffer_size);

            if (data->reqs->first == NULL) {
                data->reqs->first = (client_req*)malloc(sizeof(client_req));
                if (data->reqs->first == NULL) {
                    char* err_msg = strerror(errno);
                    ereport(ERROR, errmsg("on_read_cb: read error - %s", err_msg));
                    return ERR;
                }
                data->reqs->last = data->reqs->first;
            } else {
                data->reqs->last->next = (client_req*)malloc(sizeof(client_req));
                if (data->reqs->last->next  == NULL) {
                    char* err_msg = strerror(errno);
                    ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                    return ERR;
                }
                data->reqs->last = data->reqs->last->next;
            }
            data->reqs->last->next = NULL;
            data->reqs->last->argc = data->parsing.parsing_num;
            data->reqs->last->argv = malloc(data->reqs->last->argc * sizeof(char*));
            if (data->reqs->last->argv == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                return ERR;
            }
            data->parsing.parsing_num = 0;
            data->parsing.read_status = START_STRING_WAIT;
        } else if (c == '$' && status == START_STRING_WAIT) {

            ereport(INFO, errmsg("pars_data: 4"));
            data->parsing.read_status = NUM_WAIT;
        } else if (c == '\r' && status == NUM_WAIT) {
            ereport(INFO, errmsg("pars_data: 5"));
            check_crlf(cur_buffer_index, data->cur_buffer_size);

            data->parsing.size_str = data->parsing.parsing_num;
            data->parsing.cur_size_str = 0;
            data->parsing.parsing_str = (char*)malloc((data->parsing.size_str + 1)  * sizeof(char));
            if (data->parsing.parsing_str  == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                return ERR;
            }
            data->parsing.read_status = STRING_WAIT;
            data->parsing.parsing_num = 0;
        } else if (status == STRING_WAIT) {

            ereport(INFO, errmsg("pars_data: 6"));
            data->parsing.parsing_str[data->parsing.cur_size_str] = c;
            data->parsing.cur_size_str += 1;

            if (data->parsing.cur_size_str == data->parsing.size_str) {
                ereport(INFO, errmsg("pars_data: t1"));
                data->parsing.parsing_str[data->parsing.size_str] = '\0';

                new_str = (char*)malloc((data->parsing.size_str + 1) * sizeof(char));
                if (new_str == NULL) {
                    char* err_msg = strerror(errno);
                    ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                    return ERR;
                }

                ereport(INFO, errmsg("pars_data: t2"));
                memcpy(new_str, data->parsing.parsing_str, data->parsing.size_str + 1);
                ereport(INFO, errmsg("pars_data: t3"));
                data->reqs->last->argv[data->parsing.cur_count_argv] = new_str;
                ereport(INFO, errmsg("pars_data: t4"));
                free(data->parsing.parsing_str);
                ereport(INFO, errmsg("pars_data: t5"));
                data->parsing.cur_count_argv++;
                ereport(INFO, errmsg("pars_data: t6"));
                data->parsing.size_str = data->parsing.cur_size_str = 0;

                if (data->parsing.cur_count_argv == data->reqs->last->argc) {
                    data->parsing.read_status = END;
                } else {
                    data->parsing.read_status = START_STRING_WAIT;
                }
            }
        } else if(status == END){

            ereport(INFO, errmsg("pars_data: 7"));
            replace_part_of_buffer(data, cur_buffer_index);
            return ALL;
        } else {
            ereport(ERROR, errmsg("pars_data: unknown parsing state"));
            return ERR;
        }
    }
    ereport(INFO, errmsg("pars_data: NOT_ALL"));
    return NOT_ALL;
}
