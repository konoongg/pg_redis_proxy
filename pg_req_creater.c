#include <stdlib.h>
#include <string.h>

#include "postgres.h"
#include "utils/elog.h"


#include "alloc.h"
#include "cache.h"
#include "config.h"
#include "connection.h"
#include "db.h"
#include "pg_req_creater.h"

extern config_redis config;

void init_attribute(table_attribute* attr, char* key, int key_size);
void free_attr(table_attribute* attr);
char* create_columns_name(tuple* new_tuple, int* returned_size);
char* create_columns_value(tuple* new_tuple, int* returned_size);
char* create_set_values(tuple* new_tuple, int* returned_size);

void init_attribute(table_attribute* attr, char* key, int key_size) {
    int start_index = 0;
    attr_parser state = TABLE;
    for (int cur_index = 0; cur_index < key_size; ++cur_index) {
        if (key[cur_index] == config.p_conf.delim) {
            if (state == TABLE) {
                int table_size = cur_index - start_index;
                attr->table_size = table_size;
                attr->table = wcalloc((table_size + 1) * sizeof(char));
                attr->table[table_size] = '\0';
                memcpy(attr->table, key  + start_index, cur_index - start_index);
                state = COLUMN;
                start_index = cur_index + 1;
            } else if (state == COLUMN) {
                int column_size = cur_index - start_index;
                int value_size = key_size - cur_index + 1;

                attr->column = wcalloc((column_size + 1) * sizeof(char));
                attr->column[column_size] = '\0';
                attr->value = wcalloc((value_size + 1) * sizeof(char));
                attr->value[value_size] = '\0';
                attr->column_size = column_size;
                attr->value_size = value_size;
                memcpy(attr->column, key  + start_index, column_size);
                memcpy(attr->value, key  + cur_index + 1, value_size);
                return;
            }
        }
    }
}

void free_attr(table_attribute* attr) {
    free(attr->table);
    free(attr->column);
    free(attr->value);
    free(attr);
}

req_to_db* create_pg_get(client_req* req, void* data) {
    char* req_mes;
    int size_req;
    req_to_db* new_req;
    table_attribute* attr = wcalloc(sizeof(table_attribute));

    init_attribute(attr, req->argv[1], req->argv_size[1]);
    size_req = SELECT_BASE_SIZE + attr->column_size + attr->table_size + attr->value_size + 1; // \0(+1)

    req_mes = wcalloc(size_req * sizeof(char));
    snprintf(req_mes, size_req, "SELECT * FROM %s WHERE %s = '%s';", attr->table, attr->column, attr->value);
    free_attr(attr);

    new_req = wcalloc(sizeof(req_to_db));
    new_req->next = NULL;
    new_req->req = req_mes;
    new_req->reason = WAIT_DATA;
    new_req->key = req->argv[1];
    new_req->key_size = req->argv_size[1];
    new_req->size_req = size_req;
    new_req->data = data;
    return new_req;
}

char* create_columns_name(tuple* new_tuple, int* returned_size) {
    int columns_name_index = 0;
    int columns_name_size = new_tuple->count_attr - 1; // attr1<,>attr2
    int cur_attr = 0;

    for (int i = 0; i < new_tuple->count_attr; ++i) {
        columns_name_size += new_tuple->attr_name_size[i];
    }

    char* columns_name = wcalloc((columns_name_size + 1) * sizeof(char));

    memcpy(columns_name, new_tuple->attr_name[cur_attr], new_tuple->attr_name_size[cur_attr]);
    cur_attr++;
    columns_name_index +=new_tuple->attr_name_size[cur_attr];

    for (; columns_name_index < columns_name_size;) {
        columns_name[columns_name_index] = ',';
        columns_name_index++;
        memcpy(columns_name + columns_name_index , new_tuple->attr_name[cur_attr],
                                        new_tuple->attr_name_size[cur_attr]);

        columns_name_index += new_tuple->attr_name_size[cur_attr];
        cur_attr++;
    }
    *returned_size = columns_name_size;
    columns_name[columns_name_size] = '\0';
    return columns_name;
}

char* create_columns_value(tuple* new_tuple, int* returned_size) {
    int columns_value_index = 0;
    int columns_value_size = new_tuple->count_attr - 1; // attr1<,>attr2
    int cur_attr = 0;

    for (int i = 0; i < new_tuple->count_attr; ++i) {
        ereport(INFO, errmsg("create_columns_value:  columns_value_size(%d) += new_tuple->attr_size[%d] %d",
                columns_value_size, i, new_tuple->attr_size[i]));
        columns_value_size += new_tuple->attr_size[i] + 2;
    }
    ereport(INFO, errmsg("create_columns_value:  columns_value_size(%d)",columns_value_size));

    char* columns_value = wcalloc((columns_value_size + 1) * sizeof(char));

    columns_value[columns_value_index] = '\'';
    columns_value_index++;
    memcpy(columns_value + columns_value_index, new_tuple->attr[cur_attr], new_tuple->attr_size[cur_attr]);
    columns_value_index += new_tuple->attr_size[cur_attr];
    columns_value[columns_value_index] = '\'';
    columns_value_index++;

    cur_attr++;
    while (columns_value_index < columns_value_size) {
        ereport(INFO, errmsg("create_columns_value:  columns_value_index(%d)",columns_value_index));
        columns_value[columns_value_index] = ',';
        columns_value_index++;
        columns_value[columns_value_index] = '\'';
        columns_value_index++;
        memcpy(columns_value + columns_value_index , new_tuple->attr[cur_attr],
                                        new_tuple->attr_size[cur_attr]);

        columns_value_index += new_tuple->attr_size[cur_attr];
        columns_value[columns_value_index] = '\'';
        columns_value_index++;
        cur_attr++;
    }
    *returned_size = columns_value_size;
    columns_value[columns_value_size] = '\0';
    return columns_value;
}

char* create_set_values(tuple* new_tuple, int* returned_size) {
    char* set_values;
    int cur_attr = 0;
    int set_values_index = 0;
    int set_values_size = new_tuple->count_attr - 1; // attr1<,>attr2

    for (int i = 0; i < new_tuple->count_attr; ++i) {
        set_values_size += new_tuple->attr_size[i] + new_tuple->attr_name_size[i] + 1 + 2; // <name>=<value>
    }
    set_values = wcalloc((set_values_size + 1) * sizeof(char));

    memcpy(set_values, new_tuple->attr_name[cur_attr], new_tuple->attr_name_size[cur_attr]);
    set_values_index += new_tuple->attr_name_size[cur_attr];
    set_values[set_values_index] = '=';
    set_values_index++;
    set_values[set_values_index] = '\'';
    set_values_index++;
    memcpy(set_values + set_values_index, new_tuple->attr[cur_attr], new_tuple->attr_size[cur_attr]);
    set_values_index += new_tuple->attr_size[cur_attr];
    set_values[set_values_index] = '\'';
    set_values_index++;
    cur_attr++;
    ereport(INFO, errmsg("create_pg_set: set_values_index %d", set_values_index));
    while (set_values_index < set_values_size) {
        set_values[set_values_index] = ',';
        set_values_index++;
        memcpy(set_values + set_values_index , new_tuple->attr_name[cur_attr],
                                        new_tuple->attr_name_size[cur_attr]);

        set_values_index += new_tuple->attr_name_size[cur_attr];
        set_values[set_values_index] = '=';
        set_values_index++;

        set_values[set_values_index] = '\'';
        set_values_index++;
        memcpy(set_values + set_values_index , new_tuple->attr[cur_attr],
                                        new_tuple->attr_size[cur_attr]);
        set_values_index += new_tuple->attr_size[cur_attr];

        set_values[set_values_index] = '\'';
        set_values_index++;
        cur_attr++;
    }
    *returned_size = set_values_size;
    set_values[set_values_size] = '\0';
    return set_values;
}

req_to_db* create_pg_set(client_req* req, tuple* new_tuple) {
    char* req_mes;
    int columns_name_size;
    int columns_value_size;
    int set_velues_size;
    int size_req = SET_BASE_SIZE;
    req_to_db* new_req = wcalloc(sizeof(req_to_db));
    table_attribute* attr = wcalloc(sizeof(table_attribute));

    char* set_values = create_set_values(new_tuple, &set_velues_size);
    char* columns_name = create_columns_name(new_tuple, &columns_name_size);
    char* columns_value = create_columns_value(new_tuple, &columns_value_size);
    ereport(INFO, errmsg("create_pg_set: set_values %s", set_values));
    ereport(INFO, errmsg("create_pg_set: columns_name %s", columns_name));
    ereport(INFO, errmsg("create_pg_set: columns_value %s", columns_value));
    init_attribute(attr, req->argv[1], req->argv_size[1]);

    size_req += 2 * columns_name_size + columns_value_size + set_velues_size + attr->table_size;
    req_mes = wcalloc(size_req * sizeof(char));
    snprintf(req_mes, size_req, "INSERT INTO %s (%s) VALUES (%s) ON CONFLICT (%s) DO UPDATE SET %s;",
                                        attr->table, columns_name, columns_value, columns_name, set_values);


    new_req->next = NULL;
    new_req->reason = WAIT_SYNC;
    new_req->req = req_mes;
    new_req->key = req->argv[1];
    new_req->key_size = req->argv_size[1];
    new_req->size_req = size_req;
    free_attr(attr);
    free(columns_name);
    free(columns_value);
    free(set_values);
    return new_req;
}

req_to_db* create_pg_del(client_req* req) {
    char* req_mes;
    int del_cond_index = 0;
    int size_req = TRANSACTION_SIZE;
    req_to_db* new_req = wcalloc(sizeof(req_to_db));
    table_attribute* attr = wcalloc(sizeof(table_attribute));

    for (int i = 1; i < req->argc; ++i)  {
        size_req += req->argv_size[i] + DELETE_BASE_SIZE;
    }

    ereport(INFO, errmsg("create_pg_set: size_req %d", size_req));
    req_mes = wcalloc(size_req * sizeof(char));
    memcpy(req_mes + del_cond_index, "BEGIN;", 6);
    del_cond_index += 6;
    for (int i = 1; i < req->argc; ++i) {
        char* del_req;
        int size_del_req = 0;

        init_attribute(attr, req->argv[i], req->argv_size[i]);
        size_del_req = DELETE_BASE_SIZE + attr->table_size + attr->column_size + attr->value_size;
        ereport(INFO, errmsg("create_pg_set: size_del_req %d", size_del_req));
        del_req = wcalloc(size_del_req * sizeof(char));

        snprintf(del_req, size_req, "DELETE FROM %s WHERE %s=\'%s\';", attr->table, attr->column, attr->value);
        memcpy(req_mes + del_cond_index, del_req, size_del_req);
        del_cond_index += size_del_req;
        free_attr(attr);
    }

    ereport(INFO, errmsg("create_pg_set: end del_cond_index %d", del_cond_index));
    memcpy(req_mes + del_cond_index, "end;", 4);

    new_req->next = NULL;
    new_req->req = req_mes;
    new_req->key = req->argv[1];
    new_req->key_size = req->argv_size[1];
    new_req->reason = WAIT_SYNC;
    new_req->size_req = size_req;
    return new_req;
}
