#include <stdlib.h>

#include "postgres.h"
#include "postmaster/bgworker.h"
#include "fmgr.h"
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
    strcpy(worker.bgw_library_name, "pg_redis_proxy");
    strcpy(worker.bgw_function_name, "proxy_start_work");
    strcpy(worker.bgw_name, "pg_redis_proxy");
    strcpy(worker.bgw_type, "redis proxy server");
    RegisterBackgroundWorker(&worker);
}

void clean_up(void) {
    int err;
    free(config);
    err = finish_multiplexer();
    if (err == -1) {
        ereport(ERROR, errmsg("can't correct finish_multiplexer"));
    }
    err = finish_socket();
    if (err == -1) {
        ereport(ERROR, errmsg("can't correct finish_socket"));
    }
}

void proxy_start_work(Datum main_arg) {
    int err;
    ereport(INFO, errmsg("start bg worker pg_redis_proxy"));
    config = (config_proxy*) malloc(sizeof(config_proxy));
    if (config == NULL) {
        ereport(ERROR, errmsg("can't alloc memory"));
        clean_up();
        abort();
    }
    init_config(config);
    err = run_multiplexer(config->port, config->backlog_size    );
    if (err == -1) {
        ereport(ERROR, errmsg("can't do multiplex"));
        clean_up();
        abort();
    }
}