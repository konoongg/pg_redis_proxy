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
#include <signal.h>
#include "postmaster/interrupt.h"
#include "miscadmin.h"
#include "tcop/tcopprot.h"

#include "work_with_socket/work_with_socket.h"
#include "redis_reqv_converter/redis_reqv_converter.h"
#include "configure_proxy/configure_proxy.h"
#include "work_with_db/work_with_db.h"
#include "postgres_reqv_converter/postgres_reqv_converter.h"

#ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

#define DEFAULT_PORT            (6379)
#define DEFAULT_BACKLOG_SIZE    (512)


PGDLLEXPORT void proxy_start_work(Datum main_arg);
static void register_proxy(void);

void
_PG_init(void){
    register_proxy();
}

static void
register_proxy(void){
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    strcpy(worker.bgw_library_name, "pg_redis_proxy");
    strcpy(worker.bgw_function_name, "proxy_start_work");
    strcpy(worker.bgw_name, "proxy_start_work healthcheck");
    strcpy(worker.bgw_type, "redis proxy server");
    RegisterBackgroundWorker(&worker);
}

void
proxy_start_work(Datum main_arg){
    char** command_argv = NULL;
    char* rd_answer = NULL;
    char* pg_answer = NULL;
    int command_argc = 0;
    int size_pg_answer = 0;
    int size_rd_answer = 0;
    int fd = init_redis_listener();
    ereport(LOG, errmsg("START WORKER RF"));
    if(init_table() == -1){
        ereport(ERROR, errmsg("can't init tables"));
        return;
    }
    if(init_proxy_status() == -1){
        ereport(ERROR, errmsg("can't init proxy status"));
        return;
    }
    if(init_work_with_db()){
        ereport(ERROR, errmsg("can't init work with db"));
        return;
    }
    if(fd < 0){
        return;
    }
    pqsignal(SIGTERM, die);
    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    BackgroundWorkerUnblockSignals();
    while(1){
        CHECK_FOR_INTERRUPTS();
        if (parse_cli_mes(fd, &command_argc, &command_argv) == -1){
            return;
        }
        ereport(LOG, errmsg("argc: %d", command_argc));
        for(int i = 0; i < command_argc; ++i){
            ereport(LOG, errmsg("argv[%d]: %s", i, command_argv[i]));
        }
        if (process_redis_to_postgres(command_argc, command_argv, &pg_answer, &size_pg_answer) == -1){
            ereport(ERROR, errmsg("process redis to postgres"));
            return;
        }
        if (define_type_req(pg_answer, &rd_answer, size_pg_answer, &size_rd_answer) == -1){
            ereport(ERROR, errmsg("can't translate pg_answer to rd_answer"));
            return;
        }
        if(write_data(fd, rd_answer, size_rd_answer ) == -1){
            ereport(ERROR, errmsg("can't write data on socket"));
            return;
        }
    }
    finish_work_with_db();
    for(int i = 0; i < command_argc; ++i){
        free(command_argv[i]);
    }
    free(command_argv);
    free(pg_answer);
    free(rd_answer);
}
