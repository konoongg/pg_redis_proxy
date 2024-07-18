#pragma once

#include <ev.h>
#include <stdbool.h>

#define error_event(events)     ((events) & EV_ERROR)
#define BUFFER_SIZE             (200)

enum Eread_status {
    ARRAY_WAIT,
    NUM_WAIT,
    STRING_WAIT,
    START_STRING_WAIT,
    STR_SYM_WAIT
} typedef Eread_status;

enum Eexit_status {
    ERR = -1,
    NOT_ALL,
    ALL
} typedef Eexit_status;


struct Tsocket_parsing{
    int parsing_num;
    char* parsing_str;
    int size_str;
    int cur_size_str;
} typedef  Tsocket_parsing;

struct Tsocket_data{
    char** argv;
    int argc;
    int cur_count_argv;
    int cur_buffer_size;
    Tsocket_parsing parsing;
    char read_buffer[BUFFER_SIZE];
    Eread_status read_status;
    Eexit_status exit_status;
} typedef Tsocket_data;

void parse_cli_mes(Tsocket_data* data);
void replace_part_of_buffer(Tsocket_data* data, int cur_buffer_index);
int write_data(int fd, char* mes, int count_sum);
bool socket_set_nonblock(int socket_fd);
void close_connection(EV_P_ struct ev_io *io_handle);
int get_socket(int fd);
int read_data(EV_P_ struct ev_io* io_handle, char* read_buffer, int cur_buffer_size );