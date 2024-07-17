#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>

#include "work_with_socket.h"

int cur_buffer_size = 0;
int cur_buffer_index = 0;
char read_buffer[BUFFER_SIZE];


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

int
read_data(int fd){
    int readBytes = 0;
    while(readBytes == 0){
        readBytes = read(fd, read_buffer, BUFFER_SIZE);
        cur_buffer_size = readBytes;
        cur_buffer_index = 0;
        if (readBytes == -1) {
            char* err = strerror(errno);
            ereport(ERROR, errmsg("skip_symbol: %s", err));
            return -1;
        }
    }
    return 0;
}

void
replace_part_of_buffer(void){
    memmove(read_buffer, read_buffer + cur_buffer_index, cur_buffer_size - cur_buffer_index);
    cur_buffer_size = cur_buffer_size - cur_buffer_index;
    cur_buffer_index = 0;
    ereport(LOG, errmsg("REPLACE"));
}

int
skip_symbol(int fd){
    cur_buffer_index++;
    if(cur_buffer_index >= cur_buffer_size){
        if(read_data(fd) == -1){
            return -1;
        }
    }
    return 0;
}

/* 
 * Retrieves integer value from user request
 * *2\r\n$343\r\nAAAAAAAAAAAA...
 *        ^
 * =>
 * *2\r\n$343\r\nAAAAAAAAAAAA...
 *             ^^
 *  and 343 will be returned as result
 */
int
parse_num(int fd, read_status status){
    int num = 0;
    char c = read_buffer[cur_buffer_index];
    ereport(LOG, errmsg("START PARS INT"));
    if(status != NUM_WAIT){
        return -1;
    }

	// reading any chars as digits until \r symbol
    while(c != 13) {
        ereport(LOG, errmsg("SYM NUM : %c %d", c, c));
        num = (num * 10) + (c - '0');
        cur_buffer_index++;
        if(cur_buffer_index >= cur_buffer_size){
            if(read_data(fd) == -1){
                return -1;
            }
        }
        c = read_buffer[cur_buffer_index];
    }

    //skip \r
    if (skip_symbol(fd) == -1){
        return -1;
    }
    ereport(LOG, errmsg("RETURN NUM : %d  index: %d", num, cur_buffer_index));
    return num;
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
parse_string(int fd, char** arg, int* cur_count_argv){
    char c;
    int cur_index = 0;
    int string_size;

    // skipping $
    cur_buffer_index++;
    if(cur_buffer_index >= cur_buffer_size){
        if(read_data(fd) == -1){
            return -1;
        }
    }

    string_size = parse_num(fd, NUM_WAIT); // what if negative?
	// skipping \n	
    cur_buffer_index++;
    *arg = (char*)malloc((string_size + 1) * sizeof(char));
    if(*arg == NULL){
        ereport(LOG, errmsg("ERROR MALLOC"));
        return -1;
    }
    (*arg)[string_size] = '\0';
    ereport(LOG, errmsg("START PARS STRING, size string: %d", string_size));
    while(cur_index != string_size){
        c = read_buffer[cur_buffer_index];
        ereport(LOG, errmsg("SYM : %c %d", c, cur_index));
        (*arg)[cur_index] = c;
        cur_index++;
        cur_buffer_index++;
        if(cur_buffer_index >= cur_buffer_size){
            ereport(LOG, errmsg("BIG cur_buffer_index %d %d", cur_buffer_index, cur_buffer_size));
            if(read_data(fd) == -1){
                return -1;
            }
        }
    }
    // skipping \r
    if (skip_symbol(fd) == -1){
        return -1;
    }
    (*cur_count_argv)--;
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
int
parse_cli_mes(int fd, int* command_argc, char*** command_argv){
    read_status status = ARRAY_WAIT;
    int cur_count_argv;
    ereport(LOG, errmsg("START MESSAGE PARSING %d %d", cur_buffer_index, cur_buffer_size));
    if(cur_buffer_size == 0){
        if(read_data(fd) == -1){
            return -1;
        }
    }
    while(1){
        char c = read_buffer[cur_buffer_index];
        if(c == '*' && status == ARRAY_WAIT){
            status = NUM_WAIT;

	    // moving to first sign of number
            cur_buffer_index++;
            if(cur_buffer_index >= cur_buffer_size){
                if(read_data(fd) == -1){
                    return -1;
                }
            }
	    // parsing number that (should) come after asterisk
            cur_count_argv = *command_argc = parse_num(fd, status);
            ereport(LOG, errmsg("count: %d", *command_argc));
            *command_argv = (char**)malloc(*command_argc * sizeof(char*));
            if(*command_argv == NULL){
                ereport(LOG, errmsg("ERROR MALLOC"));
                return -1;
            }
            status = STRING_WAIT;
            ereport(LOG, errmsg("FINISH NUM PARSING"));
        }
        else if(c == '$' && status == STRING_WAIT){
            parse_string(fd, &((*command_argv)[*command_argc - cur_count_argv]), &cur_count_argv);
            ereport(LOG, errmsg("FINISH STRING PARSING"));
        }

		// skipping \n after string
        cur_buffer_index++;
        if(cur_count_argv == 0){
            replace_part_of_buffer();
            ereport(LOG, errmsg("FINISH  ARGV PARSING"));
            return 0;
        }
        if(cur_buffer_index >= cur_buffer_size){
            ereport(LOG, errmsg("BIG cur_buffer_index %d %d", cur_buffer_index, cur_buffer_size));
            if(read_data(fd) == -1){
                return -1;
            }
        }
    }
}
