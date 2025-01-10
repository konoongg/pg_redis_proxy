#ifndef SOCKET_H
#define SOCKET_H

typedef struct accept_conf accept_conf;

struct accept_conf {
    int listen_socket;
    int buffer_size;
};

int init_listen_socket(int listen_port, int backlog_size);
int finish_socket(void);
int init_socket (int socket_fd);

#endif
