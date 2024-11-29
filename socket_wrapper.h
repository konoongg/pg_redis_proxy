#ifndef SOCKET_H
#define SOCKET_H

#define BUFFER_SIZE             (200)


// parsers status
enum Eread_status {
    ARRAY_WAIT,
    NUM_OR_MINUS_WAIT, // first sym of num can be minus
    NUM_WAIT,
    STRING_WAIT, // wait \n before string size
    START_STRING_WAIT, //wait $
    STR_SYM_WAIT, // wait sym of string
    END // skip last \n \r
} typedef Eread_status;

/* status finish parsing
 * NOT_ALL - Need more data, wait data and reading
 * */
enum Eexit_status {
    ERR = -1,
    NOT_ALL,
    ALL
} typedef Eexit_status;


struct socket_parsing{
    int parsing_num;
    bool is_negative;
    char* parsing_str;
    int size_str;
    int cur_size_str;
} typedef socket_parsing;

// struct with ev_io WRITE
struct socket_write_data{
    char* answer;
    int size_answer;
} typedef socket_write_data;

// struct with ev_io read
struct socket_read_data{
    char** argv;
    int argc;
    int cur_count_argv;
    int cur_buffer_size;
    socket_parsing parsing;
    char read_buffer[BUFFER_SIZE];
    Eread_status read_status;
    Eexit_status exit_status;
} typedef socket_read_data;

// struct with connect
struct socket_data{
    socket_write_data write_data;
    socket_read_data read_data;
    struct ev_io* write_io_handle;
    struct ev_io* read_io_handle;
} typedef socket_data;


int init_listen_socket(int listen_port, int backlog_size);
int finish_socket(void);
int init_socket (void);

#endif
