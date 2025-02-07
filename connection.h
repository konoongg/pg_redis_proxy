#ifndef CONNECTION_H
#define CONNECTION_H

#include "event.h"

typedef enum conn_status conn_status;
typedef enum exit_status exit_status;
typedef enum proc_status proc_status;
typedef enum read_status  read_status;
typedef struct answer answer;
typedef struct answer_list answer_list;
typedef struct client_req client_req;
typedef struct conn_list conn_list;
typedef struct connection connection;
typedef struct event_data event_data;
typedef struct handle handle;
typedef struct io_read io_read;
typedef struct parsing parsing;
typedef struct requests requests;
typedef struct wthread wthread;

connection* create_connection(int fd, wthread* wthrd);
void add_active(connection* conn);
void add_wait(connection* conn);
void delete_active(connection* conn);
void delete_wait(connection* conn);
void event_notify(int fd);
void free_connection(connection* conn);
void init_wthread(wthread* wthrd);
void loop_step(wthread* wthrd);
void move_from_active_to_wait(connection* conn);
void move_from_wait_to_active(connection* conn);

/* status finish parsing
 * NOT_ALL - Need more data, wait data and reading
 * */
enum exit_status {
    ERR = -1,
    NOT_ALL,
    ALL
};

struct answer {
    char* answer;
    int answer_size;
    answer* next;
};

struct answer_list {
    answer* first;
    answer* last;
};

struct io_read {
    char* read_buffer;
    int buffer_size;
    int cur_buffer_size;
    parsing pars;
    requests* reqs;
};

struct event_data {
    void* data;
    handle* handle;
};

// parsers status
enum read_status {
    ARRAY_WAIT,
    ARGC_WAIT, // count argc
    NUM_WAIT,
    STRING_WAIT, // wait sym of string
    START_STRING_WAIT, //wait $
    CR, // skip \r
    LF, // WAIT \n after \r
    END // skip last \n\r
};


enum conn_status {
    ACCEPT,
    CLOSE,
    READ,
    WRITE,
    WAIT,
    PROCESS,
    NOTIFY,
    NOTIFY_DB,
    READ_DB,
    WRITE_DB,
};

struct client_req {
    char** argv;
    int argc;
    int* argv_size;
    struct client_req* next;
};


struct requests {
    client_req* last;
    client_req* first;
    int count_req;
};

struct parsing {
    char* parsing_str;
    int cur_count_argv;
    int cur_size_str;
    int parsing_num;
    int size_str;
    read_status cur_read_status;
    read_status next_read_status;
};


struct handle {
    void* handle;
};

enum proc_status {
    ALIVE_PROC,
    DEL_PROC,
    WAIT_PROC,
};

struct connection {
    bool is_wait;
    conn_status status;
    connection* next;
    connection* prev;
    int fd;
    proc_status (*proc)(wthread* wthrd, connection* data);
    event_data* r_data;
    void* data;
    event_data* w_data;
    wthread* wthrd;
};

struct conn_list {
    connection* first;
    connection* last;
};

struct wthread {
    conn_list* active;
    conn_list* wait;
    int active_size;
    int wait_size;
    int efd;
    event_loop* l;
    pthread_spinlock_t* lock;
};

#endif
