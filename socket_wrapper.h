#ifndef SOCKET_H
#define SOCKET_H

int init_listen_socket(int listen_port, int backlog_size);
int finish_socket(void);
int init_socket (void);

#endif
