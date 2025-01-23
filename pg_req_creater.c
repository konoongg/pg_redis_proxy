#include "cache.h"
#include "config.h"
#include "connection.h"
#include "db.h"
#include "pg_req_creater.h"

extern config_redis config;

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
                start_index = cur_index;
            } else if (state == COLUMN) {
                int column_size = cur_index - start_index;
                int value_size = key_size - cur_index;

                attr->column = wcalloc((column_size + 1) * sizeof(char));
                attr->column [column_size] = '\0';
                attr->value = wcalloc((value_size + 1) * sizeof(char));
                attr->value [value_size] = '\0';
                attr->column_size = column_size;
                attr->value_size = value_size;
                memcpy(attr->column, key  + start_index, cur_index - start_index);
                memcpy(attr->value, key  + cur_index, key_size - cur_index);
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

req_to_db* create_pg_get(client_req* req) {
    table_attribute* attr = wcalloc(sizeof(table_attribute));
    char* req;
    int size_req;
    init_attribute(attr, req->argv[1], req->argv_size[1]);
    size_req = SELECT_BASE_SIZE + attr->column_size + attr->table_size + attr->value_size + 1; // \0(+1)

    req = wcalloc(size_req * sizeof(char));
    snprintf(req, size_req, "SELECT * FROM %s WHERE %s = %s;", attr->table, attr->column, attr->value);
    free_attr(attr);

    req_to_db* new_req = wcalloc(sizeof(req_to_db));
    new_req->next = NULL;
    new_req->req = req;
    new_req->size_req = size_req;
    return new_req;
}

char* create_columns_name(tuple* new_tuple, int* returned_size) {
    int columns_name_size = new_tuple->count_attr - 1; // attr1<,>attr2
    for (int i = 0; i < new_tuple->count_attr; ++i) {
        columns_name_size += new_tuple->attr_name_size[i];
    }

    char* columns_name = wcalloc(columns_name_size * sizeof(char));
    int columns_name_index = 0;
    int cur_attr = 0;
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
    return columns_name;
}

char* create_columns_value(tuple* new_tuple, int* returned_size) {
    int columns_value_size = new_tuple->count_attr - 1; // attr1<,>attr2
    for (int i = 0; i < new_tuple->count_attr; ++i) {
        columns_value_size += new_tuple->attr_size[i];
    }

    char* columns_value = wcalloc(columns_value_size * sizeof(char));
    int columns_value_index = 0;
    int cur_attr = 0;
    memcpy(columns_value, new_tuple->attr[cur_attr], new_tuple->attr_size[cur_attr]);
    cur_attr++;
    columns_value_index += new_tuple->attr_name_size[cur_attr];

    for (; columns_value_index < columns_value_size;) {
        columns_value[columns_value_index] = ',';
        columns_value_index++;
        memcpy(columns_value + columns_value_index , new_tuple->attr_name[cur_attr],
                                        new_tuple->attr_name_size[cur_attr]);

        columns_value_index += new_tuple->attr_name_size[cur_attr];
        cur_attr++;
    }
    *returned_size = columns_value_size;
    return columns_value;
}

char* create_set_values(tuple* new_tuple, int* returned_size) {
    int set_values_size = new_tuple->count_attr - 1; // attr1<,>attr2
    for (int i = 0; i < new_tuple->count_attr; ++i) {
        set_values_size += new_tuple->attr_size[i] + new_tuple->attr_name_size[i] + 1; // <name>=<value>
    }
    char* set_values = wcalloc(set_values_size * sizeof(char));
    int set_values_index = 0;
    int cur_attr = 0;
    memcpy(set_values, new_tuple->attr_name[cur_attr], new_tuple->attr_name_size[cur_attr]);
    set_values_index += new_tuple->attr_name_size[cur_attr];
    set_values[set_values_index] = '=';
    set_values_index++;
    memcpy(set_values + set_values_index, new_tuple->attr[cur_attr], new_tuple->attr_size[cur_attr]);
    set_values_index += new_tuple->attr_size[cur_attr];
    cur_attr++;

    for (; set_values_index < set_values_size;) {
        set_values[set_values_index] = ',';
        set_values_index++;
        memcpy(set_values + set_values_index , new_tuple->attr_name[cur_attr],
                                        new_tuple->attr_name_size[cur_attr]);

        set_values_index += new_tuple->attr_name_size[cur_attr];
        set_values[set_values_index] = '=';
        set_values_index++;
        memcpy(set_values + set_values_index , new_tuple->attr[cur_attr],
                                        new_tuple->attr_size[cur_attr]);
        set_values_index += new_tuple->attr_size[cur_attr];
        cur_attr++;
    }
    *returned_size = set_values_size;
    return set_values;
}

req_to_db* create_pg_set(client_req* req, tuple* new_tuple) {
    table_attribute* attr = wcalloc(sizeof(table_attribute));
    char* req;
    int size_req = SET_BASE_SIZE;
    init_attribute(attr, req->argv[1], req->argv_size[1]);

    int columns_name_size;
    int columns_value_size;
    int set_velues_size;

    char* columns_name = create_columns_name(new_tuple, &columns_name_size);
    char* columns_value = create_columns_name(new_tuple, &columns_value_size);
    char* set_values = create_set_values(new_tuple, &set_velues_size);
    size_req += 2 * columns_name_size + columns_value_size + set_velues_size;
    req = wcalloc(size_req * sizeof(char));
    snprintf(req, size_req, "INSERT INTO %s (%s) VALUES (%s) ON CONFLICT (%s) DO UPDATE SET %s;",
                                        attr->table, columns_name, columns_value, columns_name, set_values);

    req_to_db* new_req = wcalloc(sizeof(req_to_db));
    new_req->next = NULL;
    new_req->req = req;
    new_req->size_req = size_req;
    free_attr(attr);
    free(columns_name);
    free(columns_value);
    free(set_values);
    return new_req;
}

req_to_db* create_pg_del(client_req* req) {
    table_attribute* attr = wcalloc(sizeof(table_attribute));
    char* req;
    int size_req = TRANSACTION_SIZE;

    for (int i = 1; i < req->argc; ++i)  {
        size_req += req->argv_size[i] + 4 + DELETE_BASE_SIZE;
    }
    req = wcalloc(size_req * sizeof(char));
    int del_cond_index = 0;
    memcpy(req + del_cond_index, "begin;", 6);
    del_cond_index += 6;
    for (int i = 1; i < req->argc; ++i) {
        char* del_req;
        int size_del_req = 0;
        init_attribute(attr, req->argv[i], req->argv_size[i]);
        size_del_req = DELETE_BASE_SIZE + attr->table_size + attr->column_size + attr->value_size;
        del_req = wcalloc(size_del_req * sizeof(char));

        snprintf(del_req, size_req, "DELETE FROM %s WHERE %s=%s;", attr->table, attr->column_size, attr->value);
        memcpy(req + del_cond_index, del_req, size_del_req);
        del_cond_index += size_del_req;
        free_attr(attr);
    }
    memcpy(req + del_cond_index, "end;", 4);


    req_to_db* new_req = wcalloc(sizeof(req_to_db));
    new_req->next = NULL;
    new_req->req = req;
    new_req->size_req = size_req;
    return new_req;
}
