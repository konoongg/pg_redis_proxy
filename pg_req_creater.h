#ifndef PG_REQ_H
#define PG_REQ_H

#include "connection.h"
#include "db.h"

#define SELECT_BASE_SIZE 26
#define SET_BASE_SIZE 56
#define DELETE_BASE_SIZE 21
#define TRANSACTION_SIZE 12

typedef enum attr_parser attr_parser;
typedef struct bd_value bd_value;
typedef struct table_attribute  table_attribute;

req_to_db* create_pg_get(client_req* req);
req_to_db* create_pg_set(client_req* req, tuple* new_tuple);
req_to_db* create_pg_del(client_req* req);

enum attr_parser {
    TABLE,
    COLUMN,
    VALUE,
};

struct table_attribute {
    char* table;
    char* column;
    char* value;

    int table_size;
    int column_size;
    int value_size;
};

struct bd_value {
    char** column_name;
    char** values;

    int* column_name_size;
    int* values_name_size;
};

#endif