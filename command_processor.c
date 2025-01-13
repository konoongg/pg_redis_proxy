#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "postgres.h"
#include "utils/elog.h"

#include "command_processor.h"
#include "connection.h"
#include "hash.h"

command_dict* com_dict;


// This is a dictionary that establishes a correspondence between a command name
// and the function that should be called as a callback when that command is received.
redis_command commands[] = {
    {"get", do_get},
    {"set", do_set},
    {"del", do_del}
};

void free_command(int hash);
void create_err(char* answer, char* err);

// It releases all resources associated with the structure describing the command
void free_command(int hash) {
    command_entry* cur_entry = com_dict->commands[hash]->first;

    while (cur_entry != NULL) {
        command_entry* new_entry = cur_entry->next;
        free(cur_entry);
        cur_entry = new_entry;
    }

    free(com_dict->commands[hash]);
}

// It initializes the command structures;
// a hash is calculated for each command name,
// and a hash map is created. This hash map stores,
// by hash, a structure containing the command name and the callback function.
int init_commands(void) {
    com_dict = (command_dict*)malloc(sizeof(command_dict));
    if (com_dict == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("init_worker: malloc error %s  - ", err_msg));
        return -1;
    }

    com_dict->hash_func = hash_pow_31_mod_100;
    com_dict->commands = (entris**)malloc(HASH_P_31_M_100_SIZE * sizeof(entris*));
    if (com_dict->commands == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("init_worker: malloc error %s  - ", err_msg));
        free(com_dict);
        return -1;
    }
    memset(com_dict->commands, 0, HASH_P_31_M_100_SIZE * sizeof(entris*));

    for (int i = 0; i < COMMAND_DICT_SIZE; ++i) {
        int hash = com_dict->hash_func(commands[i].name);

        if (com_dict->commands[hash] == NULL) {
            com_dict->commands[hash] = malloc(sizeof(entris));
            if (com_dict->commands[hash] == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("init_worker: malloc error %s  - ", err_msg));

                --i;
                for (; i >=0 ; --i) {
                    hash = com_dict->hash_func(commands[i].name);
                    free_command(hash);
                }
                free(com_dict);
                return -1;
            }
            com_dict->commands[hash]->first = com_dict->commands[hash]->last = malloc(sizeof(command_entry));
            if (com_dict->commands[hash]->last == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("init_worker: malloc error %s  - ", err_msg));
                free(com_dict->commands[hash]);
                --i;
                for (; i >=0 ; --i) {
                    hash = com_dict->hash_func(commands[i].name);
                    free_command(hash);
                }
                free(com_dict);
                return -1;
            }
        } else {
            com_dict->commands[hash]->last->next = malloc(sizeof(command_entry));
            if (com_dict->commands[hash]->last->next == NULL) {
                char* err_msg = strerror(errno);
                ereport(ERROR, errmsg("init_worker: malloc error %s  - ", err_msg));
                --i;
                for (; i >=0 ; --i) {
                    hash = com_dict->hash_func(commands[i].name);
                    free_command(hash);
                }
            }
        }
        com_dict->commands[hash]->last->next = NULL;
        com_dict->commands[hash]->last->command = &(commands[i]);
    }

    return 0;
}

void create_err(char* answer, char* err) {

}

int do_del (char** argv, int argc) {
    if (argc != 2) {

    }
    ereport(INFO, errmsg("do_del"));
    return 0;
}

int do_set (char** argv, int argc) {
    if (argc != 3) {

    }
    ereport(INFO, errmsg("do_set"));
    return 0;
}


int do_get (char** argv, int argc) {
    if (argc != 2) {

    }
    ereport(INFO, errmsg("do_get"));
    return 0;
}

// It receives a command with arguments,
// finds the corresponding function in the dictionary by the command name, and calls it.
// This implementation allows for quickly
// finding the function associated with a command in a short amount of time.
void process_command(client_req* req) {
    int hash = com_dict->hash_func(req->argv[0]);
    int size_command_name = strlen(req->argv[0]) + 1;
    command_entry* cur_command = com_dict->commands[hash]->first;
    while (cur_command != NULL) {

        ereport(INFO, errmsg("t0 %s %s %d", cur_command->command->name, req->argv[0], size_command_name));
        if (strncmp(cur_command->command->name, req->argv[0], size_command_name) == 0) {
            int err = cur_command->command->func(req->argv);
            if (err != 0) {
                ereport(INFO, errmsg("process_command: func err"));
            }
        }
        ereport(INFO, errmsg("t3"));
        cur_command = cur_command->next;
    }
}