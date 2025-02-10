// #include <stdlib.h>
// #include <string.h>

// #include "postgres.h"
// #include "utils/elog.h"


// #include "alloc.h"
// #include "cache.h"
// #include "config.h"
// #include "connection.h"
// #include "db.h"
 #include "pg_req_creater.h"

extern config_redis config;

void init_bd_req_attr(bd_req_attr* attr, char* key, int key_size);
void free_bd_req_attr(bd_req_attr* attr);



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

bd_req_attr* init_bd_req_attr(char* key, int key_size) {
    bd_req_attr* attr = wcalloc(sizeof(bd_req_attr));
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
                int value_size = key_size - cur_index - 1;

                attr->column = wcalloc((column_size + 1) * sizeof(char));
                attr->column[column_size] = '\0';
                attr->value = wcalloc((value_size + 1) * sizeof(char));
                attr->value[value_size] = '\0';
                attr->column_size = column_size;
                attr->value_size = value_size;
                memcpy(attr->column, key  + start_index, column_size);
                memcpy(attr->value, key  + cur_index + 1, value_size);
                return attr;
            }
        }
    }
    return NULL;
}

void free_bd_req_attr(bd_req_attr* attr) {
    free(attr->table);
    free(attr->column);
    free(attr->value);
    free(attr);
}

char* create_pg_get(char* key, int key_Size) {
    char* bd_req;

    int size_req;

    bd_req_attr* attr = init_bd_req_attr(key, key_Size);
    size_req = SELECT_BASE_SIZE + attr->column_size + attr->table_size + attr->value_size + 1; // \0(+1)

    bd_req = wcalloc(size_req * sizeof(char));
    snprintf(bd_req, size_req, "SELECT * FROM %s WHERE %s = '%s';", attr->table, attr->column, attr->value);
    bd_req[size_req - 1] = '\0';
    free_bd_req_attr(attr);
    return bd_req;
}

char* create_pg_del(int count, char** keys, int* keys_size) {
    char* bd_req;

    int del_cond_index = 0;
    int size_req = TRANSACTION_SIZE;
    for (int i = 0; i < count; ++i)  {
        size_req += keys_size[i] + DELETE_BASE_SIZE;
    }

    bd_req = wcalloc(size_req * sizeof(char));
    memcpy(bd_req + del_cond_index, "BEGIN;", 6);
    del_cond_index += 6;
    for (int i = 0; i < count; ++i) {
        char* del_req;
        int size_del_req = 0;

        bd_req_attr* attr = init_bd_req_attr( req->argv[i], req->argv_size[i]);
        size_del_req = DELETE_BASE_SIZE + attr->table_size + attr->column_size + attr->value_size;
        del_req = wcalloc(size_del_req * sizeof(char));
        snprintf(del_req, size_req, "DELETE FROM %s WHERE %s=\'%s\';", attr->table, attr->column, attr->value);
        memcpy(bd_req + del_cond_index, del_req, size_del_req);
        del_cond_index += size_del_req;
        free_bd_req_attr(attr);
        free(del_req);
    }

    memcpy(bd_req + del_cond_index, "end;", 4);
    return bd_req;
}

char* create_pg_set(char*  table, cache_data* data) {
    char* bd_req;
    int size_req = SET_BASE_SIZE;

    int columns_name_size = data->values->count_attr - 1 ; // count ','
    int columns_value_size = data->values->count_attr - 1; // count ','
    int set_values_size = data->values->count_attr - 1; // count ',';
    int count_attr = data->values->count_attr;

    for (int i = 0; i < count_attr; ++i) {
        columns_name_size += strlen(data->values->attr[i].column_name);
        set_values_size += strlen(data->values->attr[i].column_name) + 1; // +1 - =
        switch(data->values->attr[i].type) {
            case STRING:
                string* str = (string*)data->values->attr[i].data;
                columns_value_size += str->size + 2; // 'str'
                set_values_size += str->size + 2 // 'str'
                break;
            case INT:
                char str_num[MAX_STR_NUM_SIZE];
                int* num = (int)data->values->attr[i].data;
                snprintf(str_num, MAX_STR_NUM_SIZE, "%d", *num);
                set_values_size += strlen(str_num);
                columns_value_size += strlen(str_num)
                break;

        }
    }


    char* columns_name = wcalloc(columns_name_size * sizeof(char));
    char* columns_value = wcalloc(columns_value_size * sizeof(char));
    char* set_values = wcalloc(set_values_size * sizeof(char));

    int columns_name_index = 0;
    int columns_value_index = 0;
    int set_values_index = 0;
    for (int i = 0; i < count_attr; ++i) {
        int c_name_size = strlen(data->values->attr[i].column_name):
        memcpy(columns_name + columns_name_index, data->values->attr[i].column_name, c_name_size);
        columns_name_index += c_name_size;

        memcpy(set_values + set_values_index, data->values->attr[i].column_name, c_name_size);
        columns_name_index += c_name_size;

        memcpy(set_values + set_values_index, "=", 1);
        columns_name_index += 1;

        switch(data->values->attr[i].type) {
            case STRING:
                string* str = (string*)data->values->attr[i].data;
                memcpy(columns_value + columns_value_index, "\'", 1);
                columns_value_index += 1;

                memcpy(columns_value + columns_value_index, str->str, str->size);
                columns_value_index += 1;

                memcpy(columns_value + columns_value_index, "\'", 1);
                columns_value_index += 1;

                memcpy(set_values + set_values_index, "\'", 1);
                set_values_index += 1;

                memcpy(set_values + set_values_index, str->str, str->size);
                set_values_index += 1;

                memcpy(set_values + set_values_index, "\'", 1);
                set_values_index += 1;

                break;
            case INT:
                char str_num[MAX_STR_NUM_SIZE];
                int* num = (int)data->values->attr[i].data;
                snprintf(str_num, MAX_STR_NUM_SIZE, "%d", *num);

                memcpy(columns_value + columns_value_index, str_num, strlen(str_num));
                columns_value_index += strlen(str_num);

                memcpy(set_values + set_values_index, str_num, strlen(str_num));
                set_values_index += strlen(str_num);
                break;

        }

        if (i != count_attr - 1 ) {
            memcpy(columns_name + columns_name_index,",", 1);
            columns_name_index += 1;
        }
    }

    size_req += 2 * columns_name_size + columns_value_size + set_values_size + attr->table_size;
    bd_req = wcalloc(size_req * sizeof(char));
    snprintf(bd_req, size_req, "INSERT INTO %s (%s) VALUES (%s) ON CONFLICT (%s) DO UPDATE SET %s;",
                                        table, columns_name, columns_value, columns_name, set_values);


    return bd_req;
}
