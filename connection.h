#ifndef CONNECTION_H
#define CONNECTION_H

// parsers status
typedef enum Eread_status {
    ARRAY_WAIT,
    ARGC_WAIT, // count argc
    NUM_WAIT,
    STRING_WAIT, // wait sym of string
    START_STRING_WAIT, //wait $
    END // skip last \n\r
} Eread_status;

/* status finish parsing
 * NOT_ALL - Need more data, wait data and reading
 * */
typedef enum Eexit_status {
    ERR = -1,
    NOT_ALL,
    ALL
} Eexit_status;

typedef struct socket_parsing{
    char* parsing_str;
    int cur_size_str;
    int parsing_num;
    int size_str;
    int cur_count_argv;
    Eread_status read_status;
} socket_parsing;

// struct with ev_io WRITE
typedef struct socket_write_data{
    char* answer;
    int size_answer;
} socket_write_data;

typedef struct client_req {
    char** argv;
    int argc;
    struct client_req* next;
} client_req;

typedef struct requests {
    client_req* last;
    client_req* first;
    int count_req;
} requests;

// struct with ev_io read
struct socket_read_data{
    char* read_buffer;
    int buffer_size;
    int cur_buffer_size;
    requests* reqs;
    socket_parsing parsing;
} typedef socket_read_data;

// struct with connect
struct socket_data{
    socket_write_data* write_data;
    socket_read_data* read_data;
    struct ev_io* write_io_handle;
    struct ev_io* read_io_handle;
} typedef socket_data;


#endif