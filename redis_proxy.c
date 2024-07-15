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

#include "redis_reqv_parser/redis_reqv_parser.h"
#include "redis_reqv_converter/redis_reqv_converter.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define DEFAULT_PORT            (6379)
#define DEFAULT_BACKLOG_SIZE    (512)

static int init_redis_listener(void);
PGDLLEXPORT void proxy_start_work(Datum main_arg);
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
        if (parse_cli_mes(fd, &command_argc, &command_argv) == -1){
            return;
        }
        ereport(LOG, errmsg("argc: %d", command_argc));
        for(int i = 0; i < command_argc; ++i){
            ereport(LOG, errmsg("argv[%d]: %s", i, command_argv[i]));
        }
        process_redis_to_postgres(command_argc, command_argv);
    }
}

static int
init_redis_listener(void){
    int listen_socket;
    int client_socket;
    int opt;
    struct sockaddr_in sockaddr;
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == -1) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        return -1 ;
    }
    opt = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        return -1;
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(DEFAULT_PORT);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        char * err = strerror(errno);
        ereport(ERROR, errmsg("bind() error: %s %d", err, listen_socket));
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
