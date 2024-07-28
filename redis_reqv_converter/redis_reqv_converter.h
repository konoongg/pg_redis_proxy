#pragma once

#define MAX_ERROR_SIZE (256)

enum ErrorType {
    ERROR_COMMAND_NOT_FOUND,
    ERROR_WRONG_ARGUMENTS_COUNT
} typedef ErrorType;

int process_redis_to_postgres(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer);
int process_set(char* key, char* value, char** pg_answer, int* size_pg_answer);
int process_get(char* key, char** pg_answer, int* size_pg_answer);
int process_del(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer);
int process_command(int command_argc, char** command_argv);
int process_ping(char** pg_answer, int* size_pg_answer);
int process_error(char** pg_answer, int* size_pg_answer, ErrorType type);
void to_big_case(char* string);

