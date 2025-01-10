#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "postgres.h"
#include "utils/elog.h"

#include "command_processor.h"
#include "hash.h"

command_dict* com_dict;

redis_command commands[] = {
    {"get", do_get},
    {"set", do_set},
    {"del", do_del}
};

void free_command(int hash) ;

void free_command(int hash) {
    command_entry* cur_entry = com_dict->commands[hash]->first;

    while (cur_entry != NULL) {
        command_entry* new_entry = cur_entry->next;
        free(cur_entry);
        cur_entry = new_entry;
    }

    free(com_dict->commands[hash]);
}

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

int do_del (char** argv) {
    ereport(INFO, errmsg("do_del"));
    return 0;
}

int do_set (char** argv) {
    ereport(INFO, errmsg("do_set"));
    return 0;
}


int do_get (char** argv) {
    ereport(INFO, errmsg("do_get"));
    return 0;
}


int process_command(char** argv, int argc) {
    int hash = com_dict->hash_func(argv[0]);
    int size_command_name = strlen(argv[0]) + 1;
    command_entry* cur_command = com_dict->commands[hash]->first;
    while (cur_command != NULL) {
        cur_command = cur_command->next;
        if (strncpy(cur_command->command->name, argv[0], size_command_name)) {
            int err = cur_command->command->func(argv);
            return err;
        }
    }
    return -1;
}