#include <stdlib.h>
#include <unistd.h>

#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "command_processor.h"
#include "config.h"
#include "db.h"
#include "socket_wrapper.h"
#include "worker.h"

PG_MODULE_MAGIC;

void _PG_init(void);
static void register_proxy(void);
PGDLLEXPORT void proxy_start_work(Datum main_arg);
void clean_up(void);

config_redis config;

void _PG_init(void) {
    register_proxy();
}

static void register_proxy(void) {
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time =  BgWorkerStart_RecoveryFinished;
    strncpy(worker.bgw_library_name, "pg_redis_proxy", 15);
    strncpy(worker.bgw_function_name, "proxy_start_work", 17);
    strncpy(worker.bgw_name, "pg_redis_proxy", 15);
    strncpy(worker.bgw_type, "redis proxy server", 19);
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    RegisterBackgroundWorker(&worker);
}

void proxy_start_work(Datum main_arg) {
    ereport(INFO, errmsg("start bg worker pg_redis_proxy pid: %d", getpid()));
    init_config();
    if (init_commands() != 0) {
        ereport(ERROR, errmsg("proxy_start_work: can't init_commands"));
        abort();
    }
    ereport(INFO, errmsg("finish init commands"));
    if (init_cache() != 0) {
        ereport(ERROR, errmsg("proxy_start_work: can't init_cache"));
        abort();
    }
    ereport(INFO, errmsg("finish init cache"));
    init_db_worker();
    ereport(INFO, errmsg("finish init db worker"));
    if (init_workers() != 0) {
        ereport(ERROR, errmsg("proxy_start_work: can't init_workers"));
        abort();
    }
}
