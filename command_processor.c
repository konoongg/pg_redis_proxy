#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "command_processor.h"
#include "hash.h"
#include "query_cache_controller.h"
#include "resp_creater.h"

extern config_redis config;
extern default_resp_answer def_resp;
command_dict* com_dict;

process_result do_del(client_req* req, answer* answ, db_connect* db_conn);
process_result do_get(client_req* req, answer* answ, db_connect* db_conn);
process_result do_set(client_req* req, answer* answ, db_connect* db_conn);

redis_command commands[] = {
    {"del", do_del},
    {"get", do_get},
    {"set", do_set},
};

req_table* create_req(char* value, int value_size) {
    req_table* req = wcalloc(sizeof(req_table));
    int start_pos;
    int cur_count_attr;

    req->count_args = 1;
    for (int i = 0; i < value_size; ++i) {
        if (value[i] == config.p_conf.delim) {
            req->count_attr++;
        }
    }

    req->columns = wcalloc(req->count_attr * sizeof(req_column));

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

            req->columns[cur_count_attr].column_name = wcalloc(attr_name_size  * sizeof(char));
            req->columns[cur_count_attr].data = wcalloc( attr_size * sizeof(char));

            memcpy(req->columns[cur_count_attr].column_name, value + start_pos, attr_name_size);
            memcpy(req->columns[cur_count_attr].column, value + index_delim + 1, attr_size);
            req->columns[cur_count_attr].data_size = attr_size;
            cur_count_attr++;
            start_pos = cur_pos + 1;
        }
    }
    return req;
}

void free_req(req_table* req) {
    for (int i = 0; i < req->count_args; ++i) {
        free(req->columns[i].data);
    }
    free(req->columns);
    free(req);
}

process_result do_get(client_req* req, answer* answ) {
    values* res = get_cache(req->argv[1], req->argv_size[1]);

    if (res == NULL) {
        register_command();
        return DB_REQ;
    }

    create_array_resp(res, answ);

    if (unlock_cache_basket(req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return PROCESS_ERR;
    }
    return DONE;
}

process_result do_set(client_req* req, answer* answ) {
    req_table* new_req = create_req(req->argv[2], req->argv_size[2]);
    cache_data* data = init_cache_data(req->argv[1], req->argv_size[1], new_req);
    set_cache(data);
    free_cache_data(data);
    free_req(req);

    answ->answer_size = def_resp.ok.answer_size;
    answ->answer = mcalloc(answ->answer_size  * sizeof(char));
    memcpe(answ->answer, def_resp.ok.answer, answ->answer_size);

    return DB_REQ;
}

process_result do_del(client_req* req, answer* answ) {
    int count_del = 0;
    for (int i = 1; i < req->argc; ++i) {
        count_del += delete_cache(req->argv[i], req->argv_size[i]);
    }

    register_command();
    create_num_resp(answ, count_del);
    return DB_REQ;
}

void free_command(int hash) {
    command_entry* cur_entry = com_dict->commands[hash]->first;

    while (cur_entry != NULL) {
        command_entry* new_entry = cur_entry->next;
        free(cur_entry);
        cur_entry = new_entry;
    }

    free(com_dict->commands[hash]);
}

void init_commands(void) {
    com_dict = wcalloc(sizeof(command_dict));

    com_dict->hash_func = hash_pow_31_mod_100;
    com_dict->commands = wcalloc(HASH_P_31_M_100_SIZE * sizeof(entris*));

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
}

process_result process_command(client_req* req, answer* answ) {
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
