#include <stdlib.h>

#include "fmgr.h"
#include "postgres.h"
#include "postmaster/bgworker.h"
#include "utils/elog.h"

#include "config.h"
#include "multiplexer.h"
#include "socket_wrapper.h"

PG_MODULE_MAGIC;

void _PG_init(void);
static void register_proxy(void);
PGDLLEXPORT void proxy_start_work(Datum main_arg);
void clean_up(void);

config_proxy* config = NULL;

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
    int err;
    ereport(INFO, errmsg("start bg worker pg_redis_proxy"));
    config = (config_redis*) malloc(sizeof(config_redis));
    if (config == NULL) {
        ereport(ERROR, errmsg("can't alloc memory"));
        abort();
    }
    init_config(config);
}