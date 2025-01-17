#include <stdlib.h>

#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "command_processor.h"
#include "config.h"
#include "multiplexer.h"
#include "socket_wrapper.h"
#include "worker.h"

PG_MODULE_MAGIC;

void _PG_init(void);
static void register_proxy(void);
PGDLLEXPORT void proxy_start_work(Datum main_arg);
void clean_up(void);

config_redis* config = NULL;

void _PG_init(void) {
    register_proxy();
}

static void register_proxy(void) {
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_PostmasterStart;
    strncpy(worker.bgw_library_name, "pg_redis_proxy", 15);
    strncpy(worker.bgw_function_name, "proxy_start_work", 17);
    strncpy(worker.bgw_name, "pg_redis_proxy", 15);
    strncpy(worker.bgw_type, "redis proxy server", 19);
    RegisterBackgroundWorker(&worker);
}

void proxy_start_work(Datum main_arg) {
    ereport(INFO, errmsg("start bg worker pg_redis_proxy"));
    config = (config_redis*) wcalloc(sizeof(config_redis));
    init_config(config);
    if (init_commands() != 0) {
        ereport(ERROR, errmsg("proxy_start_work: can't init_commands"));
        abort();
    }
    ereport(INFO, errmsg("finish init commands"));
    if (init_cache(&config->c_conf) != 0) {
        ereport(ERROR, errmsg("proxy_start_work: can't init_cache"));
        abort();
    }
    ereport(INFO, errmsg("finish init cache"));
    if (init_workers(&config->worker_conf) != 0) {
        ereport(ERROR, errmsg("proxy_start_work: can't init_workers"));
        abort();
    }
}
