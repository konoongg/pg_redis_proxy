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
typedef struct handle handle;
typedef struct parsing parsing;
typedef struct read_data read_data;
typedef struct requests requests;
typedef struct write_data write_data;
typedef struct wthread wthread;

connection* create_connection(int fd);
void add_active(connection* conn);
void add_wait(connection* conn);
void delete_active(connection* conn);
void delete_wait(connection* conn);
void free_connection(connection* conn);

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

// struct with ev_io WRITE
struct write_data {
    answer_list* answers;
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

// struct with ev_io read
struct read_data {
    char* read_buffer;
    int buffer_size;
    int cur_buffer_size;
    parsing pars;
    requests* reqs;
    handle* handle;
    //struct ev_io* read_io_handle;
};

enum proc_status {
    ALIVE_PROC,
    DEL_PROC,
    WAIT_PROC,
};

struct connection {
    wthread* wthrd;
    connection* next;
    connection* prev;
    conn_status status;
    proc_status (*proc)(wthread* wthrd, connection* data);
    read_data* r_data;
    write_data* w_data;
    bool is_wait;
    int fd;
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
};

#endif
