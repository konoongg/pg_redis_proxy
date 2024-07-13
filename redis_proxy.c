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

void
parse_string(int fd, char** arg, int* cur_count_argv){
    char c;
    int cur_index = 0;
    int readBytes;
    int string_size = parse_num(fd, NUM_WAIT);
    *arg = (char*)malloc(string_size * sizeof(char));
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

//согласно протоколу RESP клиент отnравляет только массив байт-безопасных строк
void
parse_cli_mes(int fd, int* command_argc, char*** command_argv){
    char c;
    read_status status = ARRAY_WAIT;
    //сколкьо осталось считать аргументов
    int cur_count_argv;
    int readBytes;
    ereport(LOG, errmsg("START MESSAGW PARSING"));
    while(1){
        readBytes = read(fd, &c, 1);
        if (readBytes == -1) {
            char * err = strerror(errno);
            ereport(ERROR, errmsg("read(): %s", err));
        }
        if(readBytes == 1){
            ereport(LOG, errmsg("SYM : %c", c));
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
    ereport(LOG, errmsg("START WORKER"));
    if(fd < 0){
        return;
    }
    while(1){
        parse_cli_mes(fd, &command_argc, &command_argv);
        ereport(LOG, errmsg("argc: %d", command_argc));
        for(int i = 0; i < command_argc; ++i){
            ereport(LOG, errmsg("argv[%d]: %s", i, command_argv[i]));
        }
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