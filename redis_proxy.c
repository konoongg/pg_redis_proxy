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
#include <netinet/in.h>
#include <ev.h>
#include <errno.h>

#include "work_with_socket/work_with_socket.h"
#include "redis_reqv_converter/redis_reqv_converter.h"
#include "configure_proxy/configure_proxy.h"
#include "work_with_db/work_with_db.h"
#include "postgres_reqv_converter/postgres_reqv_converter.h"

#ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

// #define DEFAULT_PORT            (6379)
// #define DEFAULT_BACKLOG_SIZE    (512)


PGDLLEXPORT void proxy_start_work(Datum main_arg);
static void register_proxy(void);
static void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents);
static void on_read_cb(EV_P_ struct ev_io* io_handle, int revents);

static void
on_read_cb(EV_P_ struct ev_io* io_handle, int revents){
    Tsocket_data* data;
    ssize_t res = 0;
    char* pg_answer;
    char* rd_answer;
    int size_pg_answer = 0;
    int size_rd_answer = 0;
    if (error_event(revents)) {
        close_connection(loop, io_handle);
        return;
    }
    data = (Tsocket_data*)io_handle->data;
    res = read_data(loop, io_handle, data->read_buffer, data->cur_buffer_size);
    ereport(LOG, errmsg("START MESSAGE PARSING %d",  data->cur_buffer_size));
    if(res == -1){
        return;
    }
    if(data->exit_status == ALL){
        data->read_status = ARRAY_WAIT;
        for(int i = 0; i < data->argc; ++i){
            free(data->argv[i]);
        }
        free(data->argv);
        data->argv = NULL;
        data->argc = -1;
        data->parsing.parsing_str = NULL;
        data->exit_status = NOT_ALL;
    }
    data->cur_buffer_size += res;
    ereport(LOG, errmsg("RES: %ld", res));
    parse_cli_mes(data);
    if(data->exit_status != ALL){
        return;
    }
    if (process_redis_to_postgres(data->argc, data->argv, &pg_answer, &size_pg_answer) == -1){
        ereport(ERROR, errmsg("process redis to postgres"));
        return;
    }
    if (define_type_req(pg_answer, &rd_answer, size_pg_answer, &size_rd_answer) == -1){
            ereport(ERROR, errmsg("can't translate pg_answer to rd_answer"));
            return;
    }
    if(write_data(io_handle->fd, rd_answer, size_rd_answer ) == -1){
        ereport(ERROR, errmsg("can't write data on socket"));
        return;
    }
    free(pg_answer);
    free(rd_answer);
}

static void
on_accept_cb(EV_P_ struct ev_io* io_handle, int revents) {
    int socket_fd = -1;
    struct ev_io* client_io_handle;
    Tsocket_data* data = (Tsocket_data*) malloc(sizeof(Tsocket_data));
    if(data == NULL){
        ereport(ERROR, errmsg("CAN'T MALLOC"));
        return;
    }
    socket_fd = get_socket(io_handle->fd);
    if(socket_fd == -1){
        return;
    }
    client_io_handle = (struct ev_io*)malloc(sizeof(struct ev_io));
    if (!client_io_handle) {
        ereport(ERROR, errmsg( "CAN't CREATE CLIENT %d", socket_fd));
        close(socket_fd);
        return;
    }
    data->read_status = ARRAY_WAIT;
    data->cur_buffer_size = 0;
    data->argc = -1;
    data->argv = NULL;
    data->exit_status = NOT_ALL;
    data->parsing.parsing_str = NULL;
    client_io_handle->data = (void*)data;
    ev_io_init(client_io_handle, on_read_cb, socket_fd, EV_READ);
    ev_io_start(loop, client_io_handle);
}

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
    struct ev_io* accept_io_handle;
    int listen_socket;
    int opt;
    struct sockaddr_in sockaddr;
    struct ev_loop* loop;
    ProxyConfiguration config = init_configuration();

    ereport(LOG, errmsg("START WORKER RF"));
    if(init_table(config) == -1){
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
    loop = ev_default_loop(0);
    if(!loop) {
        ereport(ERROR, errmsg("cannot create libev default loop"));
        return;
    }
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == -1) {
        char* err  =  strerror(errno);
        ereport(ERROR, errmsg(" err socket(): %s", err));
        return ;
    }
    if (!socket_set_nonblock(listen_socket)) {
        ereport(ERROR, errmsg("for listen socket set nonblocking mode fail"));
        close(listen_socket);
        ev_loop_destroy(loop);
        return;
    }
    opt = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        char* err  =  strerror(errno);
        ereport(ERROR, errmsg("socket(): %s", err));
        close(listen_socket);
        ev_loop_destroy(loop);
        return;
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(config.port);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        char* err  =  strerror(errno);
        ereport(ERROR, errmsg("bind() error: %s %d", err, listen_socket));
        close(listen_socket);
        ev_loop_destroy(loop);
        return;
    }
    if(listen(listen_socket, config.backlog_size)) {
        char* err  =  strerror(errno);
        ereport(ERROR, errmsg("listen() error: %s", err));
        close(listen_socket);
        ev_loop_destroy(loop);
        return;
    }
    accept_io_handle = (struct ev_io *)malloc(sizeof(struct ev_io));
    if (!accept_io_handle) {
        ereport(ERROR, errmsg("cannot create io handle for listen socket"));
        close(listen_socket);
        ev_loop_destroy(loop);
        return;
    }
    ev_io_init(accept_io_handle, on_accept_cb, listen_socket, EV_READ);
    ev_io_start(loop, accept_io_handle);
    ereport(LOG, errmsg("EV_IO START"));
    for(;;) {
        ev_run(loop, EVRUN_ONCE);
    }
    finish_work_with_db();
    free(accept_io_handle);
    close(listen_socket);
    ev_loop_destroy(loop);
}
