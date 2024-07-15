enum read_status {
    ARRAY_WAIT,
    NUM_WAIT,
    STRING_WAIT
} typedef read_status;

void parse_cli_mes(int fd, int* command_argc, char*** command_argv);
int parse_num(int fd, read_status status);
void parse_string(int fd, char** arg, int* cur_count_argv);
void skip_symbol(int fd);
