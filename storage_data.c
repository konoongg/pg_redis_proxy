#include <stdlib.h>
#include <string.h>

#include "libpq-fe.h"

#include "alloc.h"
#include "config.h"
#include "storage_data.h"

extern config_redis config;

value* create_copy_data(value* v) {
    int count_tuples = v->count_tuples;
    int count_fields = v->count_fields;

    value* new_v = wcalloc(sizeof(value));
    new_v->count_tuples = count_tuples;
    new_v->count_fields = count_fields;
    new_v->values = wcalloc(count_tuples * sizeof(attr*));

    for (int i = 0; i < count_tuples; ++i) {
        v->values[i] = wcalloc(count_fields * sizeof(attr));
        for (int j = 0; j < count_fields; ++j ) {
            int column_name_size = strlen(v->values[i][j].column_name) + 1;
            attr* a = &(new_v->values[i][j]);
            a->type = v->values[i][j].type;

            a->column_name = wcalloc(column_name_size * sizeof(char));
            memcpy(a->column_name, v->values[i][j].column_name, column_name_size);
            a->data = wcalloc(sizeof(db_data));

            switch (a->type) {
                case INT:
                    a->data->num = v->values[i][j].data->num;
                    break;
                case STRING:
                    int str_size = v->values[i][j].data->str.size;
                    a->data->str.str = wcalloc(str_size * sizeof(char));
                    memcpy(a->data->str.str, v->values[i][j].data->str.str, str_size);
                    a->data->str.size = str_size;
                    break;
            }
        }
    }

    return new_v;
}

void free_values(value* v) {
    int count_tuples = v->count_tuples;
    int count_field = v->count_fields;

     for (int i = 0; i < count_tuples; ++i) {
        for (int j = 0; j < count_field; ++j) {
            free(v->values[i][j].column_name);
            free(v->values[i][j].data);
        }
        free(v->values[i]);
    }

    free(v->values);
    free(v);
}

req_table* create_req_by_resp(char* value, int value_size) {
    req_table* req = wcalloc(sizeof(req_table));
    int start_pos;
    int cur_count_attr;

    req->count_fields = 1;
    req->count_tuples = 1;
    for (int i = 0; i < value_size; ++i) {
        if (value[i] == config.p_conf.delim) {
            req->count_fields++;
        }
    }
    req->columns = wcalloc(sizeof(req_column*));
    req->columns[0] = wcalloc(req->count_fields * sizeof(req_column));

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

            req->columns[0][cur_count_attr].column_name = wcalloc(attr_name_size  * sizeof(char));
            req->columns[0][cur_count_attr].data = wcalloc(attr_size * sizeof(char));

            memcpy(req->columns[0][cur_count_attr].column_name, value + start_pos, attr_name_size);
            memcpy(req->columns[0][cur_count_attr].data, value + index_delim + 1, attr_size);
            req->columns[0][cur_count_attr].data_size = attr_size;
            cur_count_attr++;
            start_pos = cur_pos + 1;
        }
    }
    return req;
}

req_table* create_req_by_pg(PGresult* res, char* table) {
    req_table* req = wcalloc(sizeof(req_table));

    req->table = wcalloc(strlen(table) * sizeof(char));
    memcpy(req->table, table, strlen(table));

    req->count_tuples = PQntuples(res);
    req->count_fields = PQnfields(res);

    req->columns = wcalloc(req->count_tuples * sizeof(req_column*));

    for (int row = 0; row < req->count_tuples; ++row) {
        req->columns[row] = wcalloc(req->count_fields * sizeof(req_column));
        for (int column = 0; column < req->count_fields; ++column) {
            char* column_name = PQfname(res, column);
            char* value;
            int value_size;
            int column_name_size;

            if (column_name == NULL) {
                free_req(req);
                return NULL;
            }

            column_name_size = strlen(column_name);
            req->columns[row][column].column_name = wcalloc( column_name_size* sizeof(char));
            memcpy(req->columns[row][column].column_name, column_name, column_name_size);

            value = PQgetvalue(res, row, column);
            if (value == NULL) {
                free_req(req);
                return NULL;
            }
            value_size = PQgetlength(res, row, column);
            if (value_size == 0) {
                free_req(req);
                return NULL;
            }
            req->columns[row][column].data = wcalloc(value_size * sizeof(char));
            memcpy(req->columns[row][column].data, value, value_size);
            req->columns[row][column].data_size = value_size;
        }
    }

    return req;
}

void free_req(req_table* req) {
    for (int row = 0; row < req->count_tuples; ++row) {
        for (int column = 0; column < req->count_fields; ++column) {
            free(req->columns[row][column].data);
        }
        free(req->columns[row]);
    }
    free(req->columns);
    free(req);
}
