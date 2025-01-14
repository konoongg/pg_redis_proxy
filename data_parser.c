#include <errno.h>
#include <stdbool.h>

#include "postgres.h"
#include "utils/elog.h"

#include "connection.h"
#include "data_parser.h"

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
exit_status pars_data(socket_read_data* data) {
    int cur_buffer_index  = 0;
    for (; cur_buffer_index < data->cur_buffer_size; ++cur_buffer_index) {
        char c = data->read_buffer[cur_buffer_index];
        read_status cur_status = data->parsing.cur_read_status;
        char* new_str;

        ereport(INFO, errmsg("pars_data: c %d %c", c, c));

        if (c == '*' && cur_status == ARRAY_WAIT) {
            data->parsing.cur_read_status = ARGC_WAIT;
        } else if ((c >= '0' && c <= '9')  && (cur_status == NUM_WAIT || cur_status == ARGC_WAIT) ) {
            data->parsing.parsing_num = (data->parsing.parsing_num * 10) + (c - '0');
        } else if (c == '\r' && cur_status == ARGC_WAIT) {

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
                ereport(ERROR, errmsg("pars_data: malloc error - %s", err_msg));
                return ERR;
            }

            data->reqs->last->argv_size = malloc(data->reqs->last->argc * sizeof(int));
            if (data->reqs->last->argv_size == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("pars_data: malloc error - %s", err_msg));
                return ERR;
            }

            data->parsing.parsing_num = 0;
            data->parsing.next_read_status = START_STRING_WAIT;
            data->parsing.cur_read_status = LF;
        } else if (c == '\r' && cur_status == CR) {
            data->parsing.cur_read_status = LF;
        } else if (c == '\n' && cur_status == LF) {
            data->parsing.cur_read_status = data->parsing.next_read_status;
        } else if (c == '$' && cur_status == START_STRING_WAIT) {
            data->parsing.cur_read_status = NUM_WAIT;
        } else if (c == '\r' && cur_status == NUM_WAIT) {
            data->parsing.size_str = data->parsing.parsing_num;
            data->parsing.cur_size_str = 0;
            data->parsing.parsing_str = (char*)malloc((data->parsing.size_str + 1)  * sizeof(char));
            if (data->parsing.parsing_str  == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                return ERR;
            }
            data->parsing.next_read_status = STRING_WAIT;
            data->parsing.cur_read_status = LF;
            data->parsing.parsing_num = 0;
        } else if (cur_status == STRING_WAIT) {

            data->parsing.parsing_str[data->parsing.cur_size_str] = c;
            data->parsing.cur_size_str += 1;

            if (data->parsing.cur_size_str == data->parsing.size_str) {
                data->parsing.parsing_str[data->parsing.size_str] = '\0';

                new_str = (char*)malloc((data->parsing.size_str + 1) * sizeof(char));
                if (new_str == NULL) {
                    char* err_msg = strerror(errno);
                    ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                    return ERR;
                }

                memcpy(new_str, data->parsing.parsing_str, data->parsing.size_str + 1);
                data->reqs->last->argv[data->parsing.cur_count_argv] = new_str;
                data->reqs->last->argv_size[data->parsing.cur_count_argv] = data->parsing.cur_size_str;
                free(data->parsing.parsing_str);
                data->parsing.cur_count_argv++;
                data->parsing.size_str = data->parsing.cur_size_str = 0;

                if (data->parsing.cur_count_argv == data->reqs->last->argc) {
                    data->parsing.cur_count_argv = 0;
                    data->parsing.cur_read_status = CR;
                    data->parsing.next_read_status = END;
                } else {
                    data->parsing.cur_read_status = CR;
                    data->parsing.next_read_status = START_STRING_WAIT;
                }
            }
        } else if(cur_status == END){
            ereport(INFO, errmsg("pars_data: END"));
            replace_part_of_buffer(data, cur_buffer_index);
            return ALL;
        } else {
            ereport(ERROR, errmsg("pars_data: unknown parsing state"));
            return ERR;
        }
    }

    if(data->parsing.cur_read_status == END) {
        data->parsing.cur_read_status = ARRAY_WAIT;
        replace_part_of_buffer(data, cur_buffer_index);
        return ALL;
    }

    return NOT_ALL;
}
