#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>

#include "redis_reqv_parser.h"

void
skip_symbol(int fd){
    char c;
    int readBytes;
    readBytes = read(fd, &c, 1);
    if (readBytes == -1) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("skip_symbol: %s", err));
    }
}

int
parse_num(int fd, read_status status){
    int num = 0;
    char c = '0';
    int readBytes;
    ereport(LOG, errmsg("START PARS INT"));
    if(status != NUM_WAIT){
        return -1;
    }
    readBytes = read(fd, &c, 1);
    while(c != 13) {
        if (readBytes == -1) {
            char * err = strerror(errno);
            ereport(ERROR, errmsg("read(): %s", err));
        }
        if(readBytes == 1){
            ereport(LOG, errmsg("SYM NUM : %c %d", c, c));
            num = (num * 10) + (c - '0');
        }
        readBytes = read(fd, &c, 1);
    }
    //убираем символ отступа, полностью считываем блок с числом
    skip_symbol(fd);
    ereport(LOG, errmsg("RETURN NUM : %d", num));
    return num;
}

/*
 * instead of 'get' may parse something like 'getV3'
 * Possible solution: make arg of size (string_size + 1) and put \0 to the end.
 */
void
parse_string(int fd, char** arg, int* cur_count_argv){
    char c;
    int cur_index = 0;
    int readBytes;
    int string_size = parse_num(fd, NUM_WAIT); // what if negative?
    *arg = (char*)malloc((string_size + 1) * sizeof(char));
    (*arg)[string_size] = '\0';
    ereport(LOG, errmsg("START PARS STRING, size string: %d", string_size));
    while(cur_index != string_size){
        readBytes = read(fd, &c, 1);
        if (readBytes == -1) {
            char * err = strerror(errno);
            ereport(ERROR, errmsg("read(): %s", err));
        }
        if(readBytes == 1){
            ereport(LOG, errmsg("SYM : %c %d", c, cur_index));
            (*arg)[cur_index] = c;
            cur_index++;
        }
    }
    //пропускаем два символа
    skip_symbol(fd);
    skip_symbol(fd);
    (*cur_count_argv)--;
}


/*
 * согласно протоколу RESP клиент отnравляет только массив байт-безопасных строк
 * Examples:
 * *2\r\n$3\r\nget\r\n$5\r\nvalue\r\n
 * *0\r\n
 * TODO: check for correctness of user input. Maybe.
 * User input can be incorrect in 2 ways:
 * 1) Doesn't fit RESP protocol. Example: "x#324\f\r"
 * 2) Fits RESP protocol, but contains unexecutable (in conditions of proxy) Redis commands
 *    Example: (literally any command except get/set/ping/command for now)
 *
 * Message parser. Converts
*/
void
parse_cli_mes(int fd, int* command_argc, char*** command_argv){
    char c;
    read_status status = ARRAY_WAIT;
    //сколько осталось считать аргументов
    int cur_count_argv;
    int readBytes;
    ereport(LOG, errmsg("START MESSAGE PARSING"));
    while(1){
        readBytes = read(fd, &c, 1);
        if (readBytes == -1) {
            char * err = strerror(errno);
            ereport(ERROR, errmsg("read(): %s", err));
        }
        if(readBytes == 1){
            ereport(LOG, errmsg("SYM: %c", c));
            if(c == '*' && status == ARRAY_WAIT){
                status = NUM_WAIT;
                cur_count_argv = *command_argc = parse_num(fd, status);
                ereport(LOG, errmsg("count: %d", *command_argc));
                *command_argv = (char**)malloc(*command_argc * sizeof(char*));
                status = STRING_WAIT;
            }
            else if(c == '$' && status == STRING_WAIT){
                parse_string(fd, &((*command_argv)[*command_argc - cur_count_argv]), &cur_count_argv);
            }
            if(cur_count_argv == 0){
                return;
            }
        }
    }
}
