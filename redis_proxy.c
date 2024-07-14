#include "postgres.h"
#include "fmgr.h"
#include <unistd.h>
#include "postmaster/bgworker.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include "utils/rel.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>

#ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

#define DEFAULT_PORT            (6379)
#define DEFAULT_BACKLOG_SIZE    (512)
#define BUFFER_SIZE             (200)

enum read_status {
    ARRAY_WAIT,
    NUM_WAIT,
    STRING_WAIT
} typedef read_status;

static int init_redis_listener(void);
PGDLLEXPORT void proxy_start_work(Datum main_arg);
void parse_cli_mes(int fd, int* command_argc, char*** command_argv);
int parse_num(int fd, read_status status);
void parse_string(int fd, char** arg, int* cur_count_argv);
void skip_symbol(int fd);
static void register_proxy(void);

void process_redis_to_postgres(int command_argc, char** command_argv);
void process_get(int command_argc, char** command_argv);
void process_set(int command_argc, char** command_argv);
void process_command(int command_argc, char** command_argv);
void process_ping(int command_argc, char** command_argv);
void to_big_case(char* string);


void
_PG_init(void){
    register_proxy();
}

static void
register_proxy(void){
    BackgroundWorker worker;
    MemSet(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_PostmasterStart;
    strcpy(worker.bgw_library_name, "pg_redis_proxy");
    strcpy(worker.bgw_function_name, "proxy_start_work");
    strcpy(worker.bgw_name, "proxy_start_work healthcheck");
    strcpy(worker.bgw_type, "redis proxy server");
    RegisterBackgroundWorker(&worker);
}

/*
 * Processing part (Redis command to PostgreSQL query)
 * ["get", "key", "value"] =>
 * "SELECT value FROM pg_redis_table WHERE key=key"
 *
 * Warning: totally untested.
 * TODO: put all of the processing into different files.
 */

void 
process_get(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_get: %s", command_argv[0])); // plug
}

// should return +OK (or smth like that) if it worked
void 
process_set(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_get: %s", command_argv[0])); // plug
}

// TODO: improve it.
void
process_command(int command_argc, char** command_argv) {
	ereport(LOG, errmsg("IN process_command")); // plug
	// or better: this should return something like "$3\r\n(all commands supported)\r\n"
}

void
process_ping(int command_argc, char** command_argv) {
	ereport(LOG, errmsg("IN process_ping")); // plug
	// should return smth like "+PONG" probably
}

void 
to_big_case(char* string) {
	for (int i = 0; i < strlen(string); ++i) {
		if (string[i] >= 'a' && string[i] <= 'z') 
				string[i] = string[i] + ('A' - 'a');
	}
}

/*
 * High-level function which redirects processing of command arguments to
 * basic cases ("get", "set", etc.)
 * TODO: all commands. Or as many commands as possible
 */
void 
process_redis_to_postgres(int command_argc, char** command_argv) {
    if (command_argc == 0) {
        return; // nothing to process to db
    }
    ereport(LOG, errmsg("PROCESSING STARTED"));

	to_big_case(command_argv[0]); // converting to upper, since commands are in upper case

	if (!strcmp(command_argv[0], "GET")) {
    	ereport(LOG, errmsg("GET_PROCESSING: %s", command_argv[0]));

	} else if (!strcmp(command_argv[0], "SET")) {
    	ereport(LOG, errmsg("SET_PROCESSING: %s", command_argv[0]));

	} else if (!strcmp(command_argv[0], "COMMAND")) {
    	ereport(LOG, errmsg("COMMAND_PROCESSING: %s", command_argv[0]));

	} else { // command not found "exception"
    	ereport(LOG, errmsg("COMMAND NOT FOUND: %s", command_argv[0]));
	}
}


/*
 * Parsing part ("$3\r\n$3\r\nset\r\n$3\r\nkey\r\n$5value" =>
 * 		argc=3, argv=["get", "key", "value"])
 */

void skip_symbol(int fd){
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
    arg[string_size] = '\0';
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

void
proxy_start_work(Datum main_arg){
    char** command_argv;
    int command_argc;
    int fd = init_redis_listener();
    ereport(LOG, errmsg("START WORKER (v2)"));
    if(fd < 0){
        return;
    }
    while(1){
        parse_cli_mes(fd, &command_argc, &command_argv);
        ereport(LOG, errmsg("argc: %d", command_argc));
        for(int i = 0; i < command_argc; ++i){
            ereport(LOG, errmsg("argv[%d]: %s", i, command_argv[i]));
        }
        
        // Yan's part of the job
	// warning: totally undebugged	
        process_redis_to_postgres(command_argc, command_argv);
	
    }
}

static int
init_redis_listener(void){
    int listen_socket;
    int client_socket;
    struct sockaddr_in sockaddr;
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == -1) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        return -1 ;
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(DEFAULT_PORT);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("bind() error: %s", err));
        close(listen_socket);
        return -1;
    }
    if(listen(listen_socket, DEFAULT_BACKLOG_SIZE)) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("listen() error: %s", err));
        close(listen_socket);
        return -1;
    }
    client_socket = accept(listen_socket, NULL, NULL);
    return client_socket;
}
