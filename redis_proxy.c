#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "postmaster/bgworker.h"

PG_MODULE_MAGIC;

void _PG_init(void);
static void register_proxy(void);
PGDLLEXPORT void proxy_start_work(Datum main_arg);

void _PG_init(void) {
    register_proxy();
}

static void register_proxy(void) {
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_PostmasterStart;
    strcpy(worker.bgw_library_name, "pg_redis_proxy");
    strcpy(worker.bgw_function_name, "proxy_start_work");
    strcpy(worker.bgw_name, "pg_redis_proxy");
    strcpy(worker.bgw_type, "redis proxy server");
    RegisterBackgroundWorker(&worker);
}

void proxy_start_work(Datum main_arg) {
    ereport(INFO, errmsg("start bg worker pg_redis_proxy"));
}