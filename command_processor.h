#ifndef C_PROCESSOR_H
#define C_PROCESSOR_H

#include <stdint.h>

#define COMMAND_DICT_SIZE 3

int do_get(char** argv);
int do_set(char** argv);
int do_del(char** argv);

typedef struct command_dict  command_dict;
typedef struct redis_command  redis_command;
typedef struct command_entry command_entry;
typedef struct entris entris;

struct redis_command {
    char* name;
    int (*func)(char** argv);
};



struct command_dict {
    int (*hash_func)(char* key);
    entris** commands;
};

struct command_entry {
    redis_command* command;
    command_entry* next;
};

struct entris {
    command_entry* first;
    command_entry* last;
};

int init_commands(void);
int process_command(char** argv, int argc);
redis_command get_command();

#endif
