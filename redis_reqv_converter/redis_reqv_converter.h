#pragma once

int process_redis_to_postgres(int command_argc, char** command_argv, char** pg_answer, int* size_pg_answer);
int  process_set(char* key, char* value, char** pg_answer, int* size_pg_answer);
int  process_get(char* key, char** pg_answer, int* size_pg_answer);
int  process_command(int command_argc, char** command_argv);
int  process_ping(int command_argc, char** command_argv);
void to_big_case(char* string);
