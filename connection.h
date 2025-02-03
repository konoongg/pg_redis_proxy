#ifndef CONNECTION_H
#define CONNECTION_H



typedef struct wthread wthread;
typedef struct connection connection;
typedef enum conn_status conn_status;
typedef struct conn_list conn_list;
// typedef enum exit_status exit_status;
// typedef enum read_status read_status;
// typedef struct answer answer;
// typedef struct answer_list answer_list;
// typedef struct client_req client_req;
// typedef struct clinet clinet;
// typedef struct db_connect db_connect;
// typedef struct socket_data socket_data;
// typedef struct socket_parsing socket_parsing;
// typedef struct socket_read_data socket_read_data;
// typedef struct socket_write_data socket_write_data;
// typedef struct tuple tuple;

// // parsers status
// enum read_status {
//     ARRAY_WAIT,
//     ARGC_WAIT, // count argc
//     NUM_WAIT,
//     STRING_WAIT, // wait sym of string
//     START_STRING_WAIT, //wait $
//     CR, // skip \r
//     LF, // WAIT \n after \r
//     END // skip last \n\r
// };

// /* status finish parsing
//  * NOT_ALL - Need more data, wait data and reading
//  * */
// enum exit_status {
//     ERR = -1,
//     NOT_ALL,
//     ALL
// };

// struct socket_parsing {
//     char* parsing_str;
//     int cur_size_str;
//     int parsing_num;
//     int size_str;
//     int cur_count_argv;
//     read_status cur_read_status;
//     read_status next_read_status;
// };

// typedef struct client_req {
//     char** argv;
//     int* argv_size;
//     int argc;
//     struct client_req* next;
// } client_req;

// typedef struct requests {
//     client_req* last;
//     client_req* first;
//     int count_req;
// } requests;

// // struct with ev_io read
// struct socket_read_data {
//     char* read_buffer;
//     int buffer_size;
//     int cur_buffer_size;
//     requests* reqs;
//     socket_parsing parsing;
// };

// struct answer {
//     char* answer;
//     int answer_size;
//     answer* next;
// };

// struct answer_list {
//     answer* first;
//     answer* last;
// };

// // struct with ev_io WRITE
// struct socket_write_data {
//     answer_list* answers;
// };

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

// struct clinet {
//     int cur_db;
// };

// struct tuple {
//     int count_attr;
//     char** attr;
//     char** attr_name;

//     int* attr_size;
//     int* attr_name_size;
// };

enum conn_status {
    READ,
    WRITE,
    WAIT,
};

struct connection {
    connection* next;
    conn_status status;
    void* (*proc)();
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
