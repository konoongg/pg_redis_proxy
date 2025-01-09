#include <errno.h>
#include <stdbool.h>

#include "postgres.h"
#include "utils/elog.h"

#include "multiplexer.h"

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
        Eread_status status = data->read_status;
        if(c == '*' && data->read_status == ARRAY_WAIT){
            data->read_status = ARGC_WAIT;
        } else if((c >= '0' && c <= '9')  && (status == NUM_OR_MINUS_WAIT || status == NUM_WAIT || status == ARGC_WAIT) ) {
            data->parsing.parsing_num = (data->parsing.parsing_num * 10) + (c - '0');
            data->read_status = NUM_WAIT;
        } else if(c == '\r' && data->read_status == ARGC_WAIT) {
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
            data->parsing.parsing_num = 0;
        } else if (c == '-' && data->read_status == NUM_OR_MINUS_WAIT) {
            data->parsing.is_negative = true;
            data->read_status = NUM_WAIT;
        } else if(c == '\r' && data->read_status == NUM_WAIT){
            if(data->parsing.is_negative){
                data->parsing.parsing_num *= -1;
            }

            data->parsing.size_str = data->parsing.parsing_num;
            data->parsing.cur_size_str = 0;
            data->parsing.parsing_str = (char*)malloc((data->parsing.size_str + 1)  * sizeof(char));
            if (data->parsing.parsing_str  == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                return ERR;
            }
            data->read_status = STRING_WAIT;
            data->parsing.parsing_num = 0;
        } else if(c == '\n' && data->read_status == STRING_WAIT){
            data->read_status = STR_SYM_WAIT;
        } else if(data->read_status == STR_SYM_WAIT) {
            data->parsing.parsing_str[data->parsing.cur_size_str] = c;
            data->parsing.cur_size_str += 1;

            if(data->parsing.cur_size_str == data->parsing.size_str){
                data->parsing.parsing_str[data->parsing.size_str] = '\0';
                char* new_str = (char*)malloc((data->parsing.size_str + 1) * sizeof(char));
                if(new_str == NULL){
                    char* err_msg = strerror(errno);
                    ereport(ERROR, errmsg("pars_data: read error - %s", err_msg));
                    return ERR;
                }

                //data->reqs->last->[data->cur_count_argv]
                memcpy(new_str, data->parsing.parsing_str, data->parsing.size_str + 1);
                free(data->parsing.parsing_str);
                data->cur_count_argv++;
                data->parsing.size_str = data->parsing.cur_size_str = 0;
                if(data->cur_count_argv == data->argc){
                    data->read_status = END;
                }
                else{
                    data->read_status = START_STRING_WAIT;
                }
            }
        }

    }
}