#ifndef CONNECTION_H
#define CONNECTION_H

#include "event.h"

typedef enum conn_status conn_status;
typedef enum exit_status exit_status;
typedef enum read_status  read_status;
typedef struct answer answer;
typedef struct answer_list answer_list;
typedef struct client_req client_req;
typedef struct conn_list conn_list;
typedef struct connection connection;
typedef struct parsing parsing;
typedef struct read_data read_data;
typedef struct requests requests;
typedef struct write_data write_data;
typedef struct wthread wthread;

// struct db_connect {
//     int pipe_to_db[2];
//     bool finish_read;
// };

// // struct with connect
// struct socket_data {
//     socket_write_data* write_data;
//     socket_read_data* read_data;
//     struct ev_io* write_io_handle;
//     struct ev_io* read_io_handle;
// };

// struct tuple {
//     int count_attr;
//     char** attr;
//     char** attr_name;

//     int* attr_size;
//     int* attr_name_size;
// };


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
    READED,
    WRITED,
    WAIT,
};

struct client_req {
    char** argv;
    int* argv_size;
    int argc;
    struct client_req* next;
};


struct requests {
    client_req* last;
    client_req* first;
    int count_req;
};

struct parsing {
    char* parsing_str;
    int cur_size_str;
    int parsing_num;
    int size_str;
    int cur_count_argv;
    read_status cur_read_status;
    read_status next_read_status;
};


// struct with ev_io read
struct read_data {
    char* read_buffer;
    int buffer_size;
    int cur_buffer_size;
    requests* reqs;
    parsing pars;
};

struct connection {
    connection* next;
    conn_status status;
    read_data* r_data;
};

struct conn_list {
    connection* first;
    connection* last;
};

struct wthread {
    conn_list* active;
    conn_list* wait;
    int active_size;
    int efd;
    event_loop* l;
    pthread_spinlock_t* lock;
    int listen_socket;
};

#endif
