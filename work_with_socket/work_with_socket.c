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

#include "work_with_socket.h"

int cur_buffer_size = 0;
int cur_buffer_index = 0;
char read_buffer[BUFFER_SIZE];

void
close_connection(EV_P_ struct ev_io* io_handle) {
    Tsocket_data* data = (Tsocket_data*)io_handle->data;
    ev_io_stop(loop, io_handle);
    close(io_handle->fd);
    for(int i = 0; i < data->argc; ++i){
        free(data->argv[i]);
    }
    free(data->argv);
    free(data->parsing.parsing_str);
    free(data);
    free(io_handle);
}


bool
socket_set_nonblock(int socket_fd){
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}


int
write_data(int fd, char* mes, int count_sum){
    int writeBytes = 0;
    int cur_write_bytes = 0;
    while(cur_write_bytes != count_sum){
        writeBytes = write(fd, mes + cur_write_bytes, count_sum - cur_write_bytes);
        if(writeBytes == -1){
            char* err = strerror(errno);
            ereport(ERROR, errmsg("write err: %s", err));
            return -1;
        }
        cur_write_bytes += writeBytes;
    }
    return 0;
}

/*
 * According to RESP protocol, client sends only arrays of bulk strings, like:
 * *2\r\n$3\r\nget\r\n$5\r\nvalue\r\n
 * *0\r\n
 * *1\r\n$7\r\ncommand\r\n
 * For this reason, this function works only with such strings
 *
 * TODO: check for correctness of user input. Maybe.
 * User input can be incorrect in 2 ways:
 * 1) Doesn't fit RESP protocol. Example: "x#324\f\r"
 * 2) Fits RESP protocol, but contains unexecutable (in conditions of proxy) Redis commands
 *    Example: (literally any command except get/set/ping/command for now)
 *
 * Message parser. Converts requests (arrays of bulk strings) into a list of strings, which is stored at command_argv:
 * *2\r\n$3\r\nget\r\n$5\r\nvalue\r\n
 * => command_argv = ["get", "value"], command_argc = 2
*/
void
parse_cli_mes(Tsocket_data* data){
    int cur_buffer_index = 0;
    ereport(LOG, errmsg("SIZE: %d", data->cur_buffer_size));
    while(1){
        // конечные автоматы наше все
        char c = data->read_buffer[cur_buffer_index];
        ereport(LOG, errmsg("SYM %c %d ", c, c));
        if(c == '*' && data->read_status == ARRAY_WAIT){
            ereport(LOG, errmsg("START PARS INT"));
            data->read_status = NUM_WAIT;
        }
        else if(c >= '0' && c <= '9' && data->read_status == NUM_WAIT){
            ereport(LOG, errmsg("SYM NUM : %c %d", c, c));
            if(data->read_status != NUM_WAIT){
                return;
            }
            data->parsing.parsing_num = (data->parsing.parsing_num * 10) + (c - '0');
        }
        else if(c == '\r' && data->read_status == NUM_WAIT){
            ereport(LOG, errmsg("NUM: %d",data->parsing.parsing_num ));
            if(data->argc == -1){
                data->argc = data->parsing.parsing_num;
                ereport(LOG, errmsg("ARGC: %d", data->argc));
                data->cur_count_argv = 0;
                data->read_status = START_STRING_WAIT;
                data->argv = (char**)malloc(data->argc * sizeof(char*));
                if(data->argv == NULL){
                    ereport(ERROR, errmsg("CAN'T MALLOC"));
                    data->exit_status = ERR;
                    return;
                }
            }
            else{
                data->parsing.size_str = data->parsing.parsing_num;
                data->parsing.cur_size_str = 0;
                data->parsing.parsing_str = (char*)malloc((data->parsing.size_str + 1)  * sizeof(char));
                data->read_status = STRING_WAIT;
                if(data->parsing.parsing_str == NULL){
                    ereport(ERROR, errmsg("CAN'T MALLOC"));
                    data->exit_status = ERR;
                    return;
                }
            }
            data->parsing.parsing_num = 0;
        }
        else if(c == '\n' && data->read_status == START_STRING_WAIT){
            //skip
        }
        else if(c == '\n' && data->read_status == STRING_WAIT){
            data->read_status = STR_SYM_WAIT;
        }
        else if(c == '$' && data->read_status == START_STRING_WAIT){
            data->read_status = NUM_WAIT;
        }
        else if(data->read_status == STR_SYM_WAIT){
            data->parsing.parsing_str[data->parsing.cur_size_str] = c;
            data->parsing.cur_size_str += 1;
            if(data->parsing.cur_size_str == data->parsing.size_str){
                data->parsing.parsing_str[data->parsing.size_str] = '\0';
                ereport(LOG, errmsg("STR :%s cur_count_argv: %d", data->parsing.parsing_str, data->cur_count_argv));
                data->argv[data->cur_count_argv] = (char*)malloc((data->parsing.size_str + 1) * sizeof(char));
                if(data->argv[data->cur_count_argv] == NULL){
                    ereport(ERROR, errmsg("CAN'T MALLOC"));
                    data->exit_status = ERR;
                    return;
                }
                memcpy(data->argv[data->cur_count_argv], data->parsing.parsing_str, data->parsing.size_str + 1);
                ereport(LOG, errmsg("+ arg: %s  %s ", data->argv[data->cur_count_argv], data->parsing.parsing_str));
                free(data->parsing.parsing_str);
                data->cur_count_argv++;
                data->parsing.size_str = data->parsing.cur_size_str = 0;
                if(data->cur_count_argv == data->argc){
                    ereport(LOG, errmsg("ARGC final: %d", data->argc));
                    replace_part_of_buffer(data, cur_buffer_index);
                    ereport(LOG, errmsg("ARGC final: %d", data->argc));
                    data->exit_status = ALL;
                    return;
                }
                data->read_status = START_STRING_WAIT;
            }
        }
        cur_buffer_index++;
        if(cur_buffer_index >= data->cur_buffer_size){
            ereport(LOG, errmsg("NOT ALL DATA :%s %d %d", data->read_buffer, cur_buffer_index, data->cur_buffer_size));
            data->cur_buffer_size = 0;
            data->exit_status = NOT_ALL;
            return;
        }
    }
}

void
replace_part_of_buffer(Tsocket_data* data, int cur_buffer_index){
    ereport(LOG, errmsg("ARGC final1: %d", data->argc));
    memmove(data->read_buffer, data->read_buffer + cur_buffer_index, data->cur_buffer_size - cur_buffer_index);
    data->cur_buffer_size -=  cur_buffer_index;
    ereport(LOG, errmsg("REPLACE"));
}


/*
 * Retrieves string value from user request
 * Example of how it works:
 * *2\r\n$3\r\nget\r\n$5\r\nvalue\r\n
 *       ^
 * =>
 * *2\r\n$3\r\nget\r\n$5\r\nvalue\r\n
 *                    ^
 *   (*arg)="get"
 */
int
get_socket(int fd){
    int socket_fd = accept(fd, NULL, NULL);
    ereport(LOG, errmsg( "ACCEPT: %d", socket_fd));
    if (socket_fd == -1) {
        char* err = strerror(errno);
        ereport(ERROR, errmsg("on_accept_cb(): %s", err));
        return -1;
    }
    if (!socket_set_nonblock(fd)) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

int
read_data(EV_P_ struct ev_io* io_handle, char* read_buffer, int cur_buffer_size ){
    int res = read(io_handle->fd, read_buffer + cur_buffer_size, BUFFER_SIZE - cur_buffer_size);
    if (!res || res < 0) {
        close_connection(loop, io_handle);
        return - 1;
    }
    return res;
}
