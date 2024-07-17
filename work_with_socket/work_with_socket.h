#pragma once

#define BUFFER_SIZE             (200)

enum read_status {
    ARRAY_WAIT,
    NUM_WAIT,
    STRING_WAIT
} typedef read_status;

int parse_cli_mes(int fd, int* command_argc, char*** command_argv);
int parse_num(int fd, read_status status);
int parse_string(int fd, char** arg, int* cur_count_argv);
int skip_symbol(int fd);
void replace_part_of_buffer(void);
int write_data(int fd, char* mes, int count_sum);
int read_data(int fd);
