int process_redis_to_postgres(int command_argc, char** command_argv);
int process_get(int command_argc, char** command_argv);
int process_set(int command_argc, char** command_argv);
int process_command(int command_argc, char** command_argv);
int process_ping(int command_argc, char** command_argv);
void to_big_case(char* string);


// constant results for some commands:

// in fact, COMMAND returns more detailed result. Probably, its needed to expand it.

// const int SUPPORTED_COMMANDS_COUNT = 4;
// const char* SUPPORTED_COMMANDS[] = {
//     "GET", "POST", "COMMAND", "PING"
// };

const int INCORRECT_AMOUNT_OF_ARGUMENTS = 1;
const int NO_COMMAND = -1;
const int SPI_GENERAL_ERROR = 2; // when something went wrong on SPI level
const int SUCCESSFUL_EXECUTION = 0;

// const char* COMMAND_NOT_FOUND_DEFAULT_RESULT = "*0\r\n";
// const char* INCORRECT_AMOUNT_OF_ARGUMENTS = "+This command receives different amount of arguments\r\n";

// const char* COMMAND_COMMAND_RESULT = "*4\r\n$3\r\nGET\r\n$3\r\nSET\r\n$4\r\nPING\r\n$6\r\nCOMMAND\r\n";
// const char* COMMAND_PING_RESULT = "+PONG";
// const char* COMMAND_NULL_RESULT = "*0\r\n";
