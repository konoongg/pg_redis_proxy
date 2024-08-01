#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include "utils/elog.h"
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ev.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "work_with_socket.h"

int cur_buffer_size = 0;
int cur_buffer_index = 0;
char read_buffer[BUFFER_SIZE];

//removes the socket from the loop and frees all associated resources
void close_connection(EV_P_ struct ev_io* io_handle) {
    Tsocket_data* data = (Tsocket_data*)io_handle->data;
    ereport(INFO, errmsg("FINISH CONNECTED: %d", io_handle->fd));
    ev_io_stop(loop, data->write_io_handle);
    ev_io_stop(loop, data->read_io_handle);
    close(io_handle->fd);
    for(int i = 0; i < data->read_data.argc; ++i){
        ereport(INFO, errmsg("DATA: %s", data->read_data.argv[i]));
        free(data->read_data.argv[i]);
    }
    close(io_handle->fd);
    free(data->read_data.argv);
    free(data->write_data.answer);
    free(data);
    free(io_handle);
}

//create socket nonblock
bool socket_set_nonblock(int socket_fd){
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}

//write data in socket
int write_data(int fd, char* mes, int count_sum){
    int writeBytes = 0;
    writeBytes = write(fd, mes, count_sum);
    if(writeBytes == -1){
        char* err = strerror(errno);
        ereport(ERROR, errmsg("write err: %s", err));
        return -1;
    }
    return writeBytes;
}

/*
 * the function works on the basis of a finite state machine,
 * it takes a character from the buffer and, depending on the state of the machine,
 * performs the necessary actions
 * The parsing state is preserved if the data arrives incomplete;
 * the received data will be processed, and the parsing state is saved. When the remaining data arrives,
 * parsing will continue correctly.
 */
void parse_cli_mes(Tsocket_read_data* data){
    int cur_buffer_index = 0;
    ereport(NOTICE, errmsg("SIZE: %d", data->cur_buffer_size));
    while(1){
        char c = data->read_buffer[cur_buffer_index];
        ereport(DEBUG5, errmsg("SYM %c %d ", c, c));
        if(c == '*' && data->read_status == ARRAY_WAIT){
            ereport(DEBUG5, errmsg("START PARS INT"));
            data->read_status = NUM_OR_MINUS_WAIT;
        }
        else if(((c >= '0' && c <= '9') || c == '-') && data->read_status == NUM_OR_MINUS_WAIT){
            if(c >= '0' && c <= '9'){
                ereport(DEBUG5, errmsg("NUM: %d", data->parsing.parsing_num));
                data->parsing.parsing_num = (data->parsing.parsing_num * 10) + (c - '0');
                data->parsing.is_negative = false;
            }
            else if(c == '-'){
                data->parsing.is_negative = true;
            }
            data->read_status = NUM_WAIT;
        }
        else if(c >= '0' && c <= '9' && data->read_status == NUM_WAIT){
            ereport(DEBUG5, errmsg("NUM: %d", data->parsing.parsing_num));
            data->parsing.parsing_num = (data->parsing.parsing_num * 10) + (c - '0');
        }
        else if(c == '\r' && data->read_status == NUM_WAIT){
            if(data->parsing.is_negative){
                data->parsing.parsing_num *= -1;
            }
            ereport(DEBUG5, errmsg("NUM: %d", data->parsing.parsing_num));
            if(data->argc == -1){
                data->argc = data->parsing.parsing_num;
                ereport(DEBUG5, errmsg("ARGC: %d", data->argc));
                data->cur_count_argv = 0;
                data->read_status = START_STRING_WAIT;
                data->argv = (char**)malloc(data->argc * sizeof(char*));
                if(data->argv == NULL){
                    ereport(DEBUG5, errmsg("CAN'T MALLOC"));
                    data->exit_status = ERR;
                    return;
                }
            }
            else{
                data->parsing.size_str = data->parsing.parsing_num;
                data->parsing.cur_size_str = 0;
                data->parsing.parsing_str = (char*)malloc((data->parsing.size_str + 1)  * sizeof(char));
                ereport(DEBUG5, errmsg(" DATA parsing_str: %p", data->parsing.parsing_str));
                data->read_status = STRING_WAIT;
                if(data->parsing.parsing_str == NULL){
                    ereport(DEBUG5, errmsg("CAN'T MALLOC"));
                    data->exit_status = ERR;
                    return;
                }
            }
            data->parsing.parsing_num = 0;
        }
        else if(c == '\n' && data->read_status == START_STRING_WAIT){
            // skip
        }
        else if(c == '\n' && data->read_status == STRING_WAIT){
            data->read_status = STR_SYM_WAIT;
        }
        else if(c == '$' && data->read_status == START_STRING_WAIT){
            data->read_status = NUM_OR_MINUS_WAIT;
        }
        else if(data->read_status == STR_SYM_WAIT){
            data->parsing.parsing_str[data->parsing.cur_size_str] = c;
            data->parsing.cur_size_str += 1;
            if(data->parsing.cur_size_str == data->parsing.size_str){
                data->parsing.parsing_str[data->parsing.size_str] = '\0';
                ereport(DEBUG5, errmsg("STR :%s cur_count_argv: %d", data->parsing.parsing_str, data->cur_count_argv));
                data->argv[data->cur_count_argv] = (char*)malloc((data->parsing.size_str + 1) * sizeof(char));
                if(data->argv[data->cur_count_argv] == NULL){
                    ereport(DEBUG5, errmsg("CAN'T MALLOC"));
                    data->exit_status = ERR;
                    return;
                }
                memcpy(data->argv[data->cur_count_argv], data->parsing.parsing_str, data->parsing.size_str + 1);
                ereport(DEBUG5, errmsg("+ arg: %s  %s ", data->argv[data->cur_count_argv], data->parsing.parsing_str));
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
        else if(data->read_status == END){
            ereport(DEBUG5, errmsg("end: %d", c));
            if(c == '\n'){
                cur_buffer_index++;
                replace_part_of_buffer(data, cur_buffer_index);
                data->exit_status = ALL;
                return;
            }
        }
        cur_buffer_index++;
        if(cur_buffer_index >= data->cur_buffer_size){
            ereport(DEBUG5, errmsg("NOT ALL DATA: %d cur_buffer_index: %d data->cur_buffer_size:%d", data->read_status, cur_buffer_index, data->cur_buffer_size));
            data->cur_buffer_size = 0;
            data->exit_status = NOT_ALL;
            return;
        }
    }
}


// saves data to a buffer if more than one packet has been written off
void replace_part_of_buffer(Tsocket_read_data* data, int cur_buffer_index){
    ereport(DEBUG5, errmsg("ARGC final1: %d cur_buffer_size: %d", data->argc, data->cur_buffer_size));
    memmove(data->read_buffer, data->read_buffer + cur_buffer_index, data->cur_buffer_size - cur_buffer_index);
    data->cur_buffer_size -=  cur_buffer_index;
    ereport(INFO, errmsg("REPLACE: %d", data->cur_buffer_size));
}

int get_socket(int fd){
    int socket_fd = accept(fd, NULL, NULL);
    int opt;
    ereport(DEBUG5, errmsg( "ACCEPT: %d", socket_fd));
    if (socket_fd == -1) {
        char* err = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb(): %s", err));
        return -1;
    }
    if (!socket_set_nonblock(fd)) {
        close(socket_fd);
        return -1;
    }
    opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        char* err  =  strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        close(socket_fd);
        return -1;
    }
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        char* err = strerror(errno);
        ereport(ERROR, errmsg("setsockopt TCP_NODELAY: %s", err));
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

// read data in buffer
// The buffer has a constant size; if there is more data, it will be read later.
int read_data(EV_P_ struct ev_io* io_handle, char* read_buffer, int cur_buffer_size){
    int res = read(io_handle->fd, read_buffer + cur_buffer_size, BUFFER_SIZE - cur_buffer_size);
    if (!res || res < 0) {
        ereport(INFO, errmsg( "CLOS CONNCTION"));
        close_connection(loop, io_handle);
        ereport(INFO, errmsg( "SUC CLOS CONNCTION"));
        return - 1;
    }
    return res;
}