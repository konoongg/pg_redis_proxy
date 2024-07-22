#pragma once

#include <ev.h>
#include <stdbool.h>

#define error_event(events)     ((events) & EV_ERROR)
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


struct Tsocket_parsing{
    int parsing_num;
    bool is_negative;
    char* parsing_str;
    int size_str;
    int cur_size_str;
} typedef  Tsocket_parsing;

// struct with ev_io WRITE
struct Tsocket_write_data{
    char* answer;
    int size_answer;
} typedef Tsocket_write_data;

// struct with ev_io read
struct Tsocket_read_data{
    char** argv;
    int argc;
    int cur_count_argv;
    int cur_buffer_size;
    Tsocket_parsing parsing;
    char read_buffer[BUFFER_SIZE];
    Eread_status read_status;
    Eexit_status exit_status;
} typedef Tsocket_read_data;

// struct with connect
struct Tsocket_data{
    Tsocket_write_data write_data;
    Tsocket_read_data read_data;
    struct ev_io* write_io_handle;
    struct ev_io* read_io_handle;
} typedef Tsocket_data;


void parse_cli_mes(Tsocket_read_data* data);
void replace_part_of_buffer(Tsocket_read_data* data, int cur_buffer_index);
int write_data(int fd, char* mes, int count_sum);
bool socket_set_nonblock(int socket_fd);
void close_connection(EV_P_ struct ev_io *io_handle);
int get_socket(int fd);
int read_data(EV_P_ struct ev_io* io_handle, char* read_buffer, int cur_buffer_size);
