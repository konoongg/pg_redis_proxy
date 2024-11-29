#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include "postgres.h"
#include "utils/elog.h"

#include "socket_wrapper.h"


int listen_socket = -1;

//free the resources needed by the socket_wrapper
int finish_socket(void) {
    if (listen_socket != -1) {
        if (close(listen_socket) == -1) {
            char* err_msg =  strerror(errno);
            ereport(ERROR, errmsg("listen() error: %s", err_msg));
            return -1;
        }
    }
    return 0;
}

// do socket non block or return err
int socket_set_nonblock(int socket_fd){
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

//create listen tcp cosket and retrun err ro fd thos socket
int init_listen_socket(int listen_port, int backlog_size) {
    struct sockaddr_in sockaddr;
    int err;
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == -1) {
        char* err_msg  =  strerror(errno);
        ereport(ERROR, errmsg(" err socket(): %s", err_msg));
        return -1;
    }
    if (socket_set_nonblock(listen_socket) == -1) {
        ereport(ERROR, errmsg("for listen socket set nonblocking mode fail"));
        return -1;
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(listen_port);
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    err = bind(listen_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (err == -1) {
        char* err_msg =  strerror(errno);
        ereport(ERROR, errmsg("bind() error: %s %d", err_msg, listen_socket));
        return -1;
    }
    err = listen(listen_socket, backlog_size);
    if (err == -1) {
        char* err_msg  =  strerror(errno);
        ereport(ERROR, errmsg("listen() error: %s", err_msg));
        return -1;
    }
    ereport(INFO, errmsg("init listen socket"));
    return listen_socket;
}

//accept socket and configure it, use saved listen_socket
int init_socket (void) {
    int socket_fd = accept(listen_socket, NULL, NULL);
    ereport(DEBUG5, errmsg( "ACCEPT: %d", socket_fd));
    if (socket_fd == -1) {
        char* err = strerror(errno);
        ereport(WARNING, errmsg("on_accept_cb(): %s", err));
        return -1;
    }
    if (socket_set_nonblock(socket_fd) == -1) {
        ereport(WARNING, errmsg("for listen socket set nonblocking mode fail"));
        return -1;
    }
    return socket_fd;
}