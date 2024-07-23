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
#include <stdbool.h>
#include <signal.h>
#include "postmaster/interrupt.h"
#include "miscadmin.h"
#include "tcop/tcopprot.h"
#include <netinet/in.h>
#include <ev.h>
#include <errno.h>
#include "tcop/tcopprot.h"
#include "miscadmin.h"

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
static void on_accept_cb(EV_P_ struct ev_io* io_handle, int revents);
static void on_read_cb(EV_P_ struct ev_io* io_handle, int revents);


static void
on_write_cb(EV_P_ struct ev_io* io_handle, int revents){
    int byte_write;
    Tsocket_data* socket_data = (Tsocket_data*)io_handle->data;
    Tsocket_write_data* write_info = &(socket_data->write_data);
    ereport(LOG, errmsg("WRITE WORK %s %d", write_info->answer, write_info->size_answer));
    byte_write = write_data(io_handle->fd,write_info->answer, write_info->size_answer);
    if(byte_write == -1){
        return;
    }
    else if(byte_write == write_info->size_answer){
        ereport(LOG, errmsg("WRITER STOP"));
        ev_io_stop(loop, io_handle);
        write_info->size_answer = 0;
    }
    else{
        write_info->size_answer -= byte_write;
        memcpy(write_info->answer, write_info->answer + byte_write, write_info->size_answer - byte_write);
    }
}

static void
on_read_cb(EV_P_ struct ev_io* io_handle, int revents){
    Tsocket_data* socket_data;
    Tsocket_write_data* write_info;
    Tsocket_read_data* read_info;
    ev_io* write_io_handle;
    ssize_t res = 0;
    char* pg_answer = NULL;
    char* rd_answer = NULL;
    int size_pg_answer = 0;
    int size_rd_answer = 0;
    if (error_event(revents)) {
        close_connection(loop, io_handle);
        return;
    }
    socket_data = (Tsocket_data*)io_handle->data;
    write_io_handle = (ev_io*)socket_data->write_io_handle;
    write_info = &(socket_data->write_data);
    read_info = &(socket_data->read_data);
    res = read_data(loop, io_handle, read_info->read_buffer, read_info->cur_buffer_size);
    read_info->cur_buffer_size = res;
    if(res == -1){
        return;
    }
    do{
        if(read_info->exit_status == ALL){
            read_info->read_status = ARRAY_WAIT;
            for(int i = 0; i < read_info->argc; ++i){
                free(read_info->argv[i]);
            }
            free(read_info->argv);
            read_info->argv = NULL;
            read_info->argc = -1;
            read_info->parsing.parsing_str = NULL;
            read_info->parsing.parsing_num = 0;
            read_info->exit_status = NOT_ALL;
        }
        parse_cli_mes(read_info);
        if(read_info->exit_status != ALL){
            break;
        }
        if (process_redis_to_postgres(read_info->argc, read_info->argv, &pg_answer, &size_pg_answer) == -1){
            ereport(ERROR, errmsg("process redis to postgres"));
            return;
        }
        ereport(LOG, errmsg("START PR CONVERT"));
        if (define_type_req(pg_answer, &rd_answer, size_pg_answer, &size_rd_answer) == -1){
            ereport(ERROR, errmsg("can't translate pg_answer to rd_answer"));
            return;
        }
        ereport(LOG, errmsg("START WRITE"));
        if(write_info->answer == NULL){
            write_info->answer = (char*)malloc(size_rd_answer * sizeof(char));
            write_info->size_answer = size_rd_answer;
            memcpy(write_info->answer,rd_answer,size_rd_answer);
            if(write_info->answer == NULL){
                ereport(ERROR, errmsg("can't malloc"));
                return;
            }
        }
        else{
            write_info->answer = (char*)realloc(write_info->answer, write_info->size_answer + size_rd_answer * sizeof(char));
            memcpy(write_info->answer + write_info->size_answer ,rd_answer,size_rd_answer);
            write_info->size_answer += size_rd_answer;
        }
    } while(read_info->cur_buffer_size != 0);
    ereport(LOG, errmsg("answer: %s answer_size: %d rd_answer: %s", write_info->answer, write_info->size_answer, rd_answer));
    if(write_info->size_answer > 0){
        ereport(LOG, errmsg("EV_IO  wreti start"));
        ev_io_start(loop, write_io_handle);
    }
    free(pg_answer);
    free(rd_answer);
}

static void
on_accept_cb(EV_P_ struct ev_io* io_handle, int revents) {
    int socket_fd = -1;
    struct ev_io* read_io_handle;
    struct ev_io* write_io_handle;
    Tsocket_data* socket_data;
    socket_data = (Tsocket_data*)malloc(sizeof(Tsocket_data));
    if(socket_data == NULL){
        ereport(ERROR, errmsg("CAN'T MALLOC"));
        return;
    }
    socket_fd = get_socket(io_handle->fd);
    if(socket_fd == -1){
        return;
    }
    read_io_handle = (struct ev_io*)malloc(sizeof(struct ev_io));
    if (!read_io_handle) {
        ereport(ERROR, errmsg( "CAN't CREATE CLIENT %d", socket_fd));
        close(socket_fd);
        return;
    }
    write_io_handle = (struct ev_io*)malloc(sizeof(struct ev_io));
    if (!write_io_handle) {
        ereport(ERROR, errmsg( "CAN't CREATE CLIENT %d", socket_fd));
        close(socket_fd);
        return;
    }
    socket_data->write_io_handle = write_io_handle;
    socket_data->read_io_handle = read_io_handle;
    socket_data->read_data.read_status = ARRAY_WAIT;
    socket_data->read_data.cur_buffer_size = 0;
    socket_data->read_data.argc = -1;
    socket_data->read_data.argv = NULL;
    socket_data->read_data.exit_status = NOT_ALL;
    socket_data->read_data.parsing.parsing_str = NULL;
    socket_data->read_data.parsing.parsing_num = 0;
    socket_data->write_data.answer = NULL;
    socket_data->write_data.size_answer = 0;
    read_io_handle->data = (void*)socket_data;
    write_io_handle->data = (void*)socket_data;
    ev_io_init(read_io_handle, on_read_cb, socket_fd, EV_READ);
    ev_io_start(loop, read_io_handle);
    ev_io_init(write_io_handle, on_write_cb, socket_fd, EV_WRITE);
    ereport(LOG, errmsg("PTR MAIN %p fd: %d", write_io_handle, socket_fd ));
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
    sockaddr.sin_port = htons(DEFAULT_PORT);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
        char* err  =  strerror(errno);
        ereport(ERROR, errmsg("bind() error: %s %d", err, listen_socket));
        close(listen_socket);
        ev_loop_destroy(loop);
        return;
    }
    if(listen(listen_socket, DEFAULT_BACKLOG_SIZE)) {
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
    pqsignal(SIGTERM, die);
    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    BackgroundWorkerUnblockSignals();
    for(;;) {
        CHECK_FOR_INTERRUPTS();
        ev_run(loop, EVRUN_ONCE);
    }
    finish_work_with_db();
    free(accept_io_handle);
    close(listen_socket);
    ev_loop_destroy(loop);
}