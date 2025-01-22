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
    snprintf(req, size_req, "SELECT * FROM %s WHERE %s = %s", attr->table, attr->column, attr->value);
    free_attr(attr);

    req_to_db* new_req = wcalloc(sizeof(req_to_db));
    new_req->next = NULL;
    new_req->req = req;
    new_req->size_req = size_req;
    return new_req;
}
