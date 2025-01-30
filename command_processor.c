#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "command_processor.h"
#include "connection.h"
#include "db.h"
#include "hash.h"
#include "pg_req_creater.h"
#include "resp_creater.h"

extern config_redis config;
command_dict* com_dict;

process_result do_del(client_req* req, answer* answ, db_connect* db_conn);
process_result do_get(client_req* req, answer* answ, db_connect* db_conn);
process_result do_set(client_req* req, answer* answ, db_connect* db_conn);
tuple* create_tuple(char* value, int value_size);
void free_tuple(tuple* tpl);

// This is a dictionary that establishes a correspondence between a command name
// and the function that should be called as a callback when that command is received.
redis_command commands[] = {
    {"del", do_del},
    {"get", do_get},
    {"set", do_set},
};

void free_resp_answ(void* ptr) {
    answer* data = (answer*)ptr;
    free(data->answer);
    free(data);
}

tuple* create_tuple(char* value, int value_size) {
    ereport(INFO, errmsg("create_tuple: start"));
    tuple* new_tuple = wcalloc(sizeof(tuple));
    int cur_count_attr;
    int start_pos;

    new_tuple->count_attr = 1;

    for (int i = 0; i < value_size; ++i) {
        if (value[i] == config.p_conf.delim) {
            new_tuple->count_attr++;
        }
    }

    new_tuple->attr = wcalloc(new_tuple->count_attr * sizeof(char*));
    new_tuple->attr_size = wcalloc(new_tuple->count_attr * sizeof(int));

    new_tuple->attr_name = wcalloc(new_tuple->count_attr * sizeof(char*));
    new_tuple->attr_name_size = wcalloc(new_tuple->count_attr * sizeof(int));

    start_pos = 0;
    cur_count_attr = 0;
    for (int cur_pos = 0; cur_pos < value_size + 1; ++cur_pos) {
        if (value[cur_pos] == config.p_conf.delim || value[cur_pos] == '\0') {
            int index_delim;
            int attr_name_size;
            int attr_size;

            for (index_delim = start_pos; index_delim < cur_pos; ++index_delim) {
                if (value[index_delim] == ':') {
                    break;
                }
            }

            attr_name_size = index_delim - start_pos;
            attr_size = cur_pos - index_delim - 1;

            new_tuple->attr[cur_count_attr] = wcalloc(attr_size  * sizeof(char));
            new_tuple->attr_name[cur_count_attr] = wcalloc(attr_name_size  * sizeof(char));

            memcpy(new_tuple->attr_name[cur_count_attr], value + start_pos, attr_name_size);
            memcpy(new_tuple->attr[cur_count_attr], value + index_delim + 1, attr_size);
            new_tuple->attr_name_size[cur_count_attr] = attr_name_size;
            new_tuple->attr_size[cur_count_attr] = attr_size;
            cur_count_attr++;
            start_pos = cur_pos + 1;
        }
    }
    return new_tuple;
}

void free_tuple(tuple* tpl) {
    free(tpl->attr_size);
    free(tpl->attr_name_size);
    for (int i = 0; i < tpl->count_attr; ++i) {
        free(tpl->attr_name[i]);
        free(tpl->attr[i]);
    }
    free(tpl->attr);
    free(tpl->attr_name);
    free(tpl);
}

process_result do_get(client_req* req, answer* answ, db_connect* db_conn) {
    ereport(INFO, errmsg("do_get: start %d ", req->argc));
    answer* res;
    if (lock_cache_basket(req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return PROCESS_ERR;
    }

    res = get_cache(create_data(req->argv[1], req->argv_size[1], NULL, NULL));

    if (res == NULL) {
        int notify_fd = register_command(create_pg_get(req, answ), db_conn);
        if (notify_fd == -1) {
            return PROCESS_ERR;
        }
        ereport(INFO, errmsg("do_get: start sub"));
        subscribe(req->argv[1], req->argv_size[1], WAIT_DATA, notify_fd);
        ereport(INFO, errmsg("do_get: finish sub"));

        if (unlock_cache_basket(req->argv[1], req->argv_size[1]) != 0) {
            create_err_resp(answ, "ERR syntax error");
            return PROCESS_ERR;
        }
        return DB_REQ;
    }

    answ->answer = wcalloc(res->answer_size * sizeof(char));
    memcpy(answ->answer, res->answer, res->answer_size);
    answ->answer_size = res->answer_size;

    if (unlock_cache_basket(req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return PROCESS_ERR;
    }
    return DONE;
}

process_result do_set(client_req* req, answer* answ, db_connect* db_conn) {
    ereport(INFO, errmsg("do_set: start %d ", req->argc));
    answer* cache_res;
    answer* new_answer = wcalloc(sizeof(answer));
    int  set_res = 0;
    process_result proccess_res;

    tuple* new_tuple = create_tuple(req->argv[2], req->argv_size[2]);

    answer* new_resp_array = wcalloc(sizeof(answer));
    create_array_bulk_string_resp(new_resp_array, new_tuple->count_attr, new_tuple->attr, new_tuple->attr_size) ;

    if (lock_cache_basket(req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return PROCESS_ERR;
    }

    cache_res = get_cache(create_data(req->argv[1], req->argv_size[1], NULL, NULL));

    if (cache_res == NULL) {

        ereport(INFO, errmsg("do_set: cache_res == NULL"));
        init_array_by_elem(new_answer, 1, new_resp_array);
    } else {
        ereport(INFO, errmsg("do_set: cache_res != NULL"));
        int array_size = get_array_size(cache_res);
        init_array_by_elem(new_answer, array_size, new_resp_array);
    }

    set_res = set_cache(create_data(req->argv[1], req->argv_size[1], new_answer, free_resp_answ));

    create_simple_string_resp(answ, "OK");

    if (unlock_cache_basket(req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return PROCESS_ERR;
    }

    if (set_res  == -1 ) {
        abort();
    }

    if (config.c_conf.mode == NO_SYNC) {;
        proccess_res = DONE;
    } else {
        int notify_fd = register_command(create_pg_set(req, new_tuple), db_conn);
        if (notify_fd == -1) {
            proccess_res = PROCESS_ERR;
        }
        subscribe(req->argv[1], req->argv_size[1], WAIT_SYNC, notify_fd);
        proccess_res = DB_REQ;
    }

    free_tuple(new_tuple);
    return proccess_res;
}

process_result do_del(client_req* req, answer* answ, db_connect* db_conn) {
    int count_del = 0;
    process_result process_res = NONE;
    ereport(INFO, errmsg("do_del: start %d ", req->argc));
    for (int i = 1; i < req->argc; ++i) {

        ereport(INFO, errmsg("do_del: try lock"));
        if (lock_cache_basket(req->argv[i], req->argv_size[i]) != 0) {
            create_err_resp(answ, "ERR syntax error");
            return PROCESS_ERR;
        }

        ereport(INFO, errmsg("do_del: suc lock"));
        count_del += delete_cache(req->argv[i], req->argv_size[i]);

        if (unlock_cache_basket(req->argv[i], req->argv_size[i]) != 0) {
            create_err_resp(answ, "ERR syntax error");
            return PROCESS_ERR;
        }
        ereport(INFO, errmsg("do_del: unlock"));
    }

    if (config.c_conf.mode == NO_SYNC) {
        process_res =  DONE;
    } else if (config.c_conf.mode == ALL_SYNC) {
        int notify_fd = register_command(create_pg_del(req), db_conn);
        if (notify_fd == -1) {
            process_res =  PROCESS_ERR;
        } else {
            subscribe(req->argv[1], req->argv_size[1], WAIT_SYNC, notify_fd);
            process_res =  DB_REQ;
        }
    }
    ereport(INFO, errmsg("do_del: count_del %d", count_del));
    create_num_resp(answ, count_del);
    return process_res;
}

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
    com_dict = wcalloc(sizeof(command_dict));

    com_dict->hash_func = hash_pow_31_mod_100;
    com_dict->commands = wcalloc(HASH_P_31_M_100_SIZE * sizeof(entris*));
    memset(com_dict->commands, 0, HASH_P_31_M_100_SIZE * sizeof(entris*));

    for (int i = 0; i < COMMAND_DICT_SIZE; ++i) {
        int hash = com_dict->hash_func(commands[i].name);

        if (com_dict->commands[hash] == NULL) {
            com_dict->commands[hash] = wcalloc(sizeof(entris));
            com_dict->commands[hash]->first = com_dict->commands[hash]->last = wcalloc(sizeof(command_entry));
        } else {
            com_dict->commands[hash]->last->next = wcalloc(sizeof(command_entry));
        }
        com_dict->commands[hash]->last->next = NULL;
        com_dict->commands[hash]->last->command = &(commands[i]);
    }

    return 0;
}

// It receives a command with arguments,
// finds the corresponding function in the dictionary by the command name, and calls it.
// This implementation allows for quickly
// finding the function associated with a command in a short amount of time.
process_result process_command(client_req* req, answer* answ, db_connect* db_conn) {
    ereport(INFO, errmsg("process_command: start"));
    int hash = com_dict->hash_func(req->argv[0]);
    int size_command_name = strlen(req->argv[0]) + 1;
    command_entry* cur_command = com_dict->commands[hash]->first;
    while (cur_command != NULL) {
        if (strncmp(cur_command->command->name, req->argv[0], size_command_name) == 0) {
            return cur_command->command->func(req, answ, db_conn);
        }
        cur_command = cur_command->next;
    }
    return PROCESS_ERR;
}

void process_err(answer* answ, char* err) {
    create_err_resp(answ, err);
}
