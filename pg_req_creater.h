#ifndef PG_REQ_H
#define PG_REQ_H

#include "connection.h"
#include "db.h"

#define SELECT_BASE_SIZE 25

typedef enum attr_parser attr_parser;
typedef struct table_attribute  table_attribute;

req_to_db* create_pg_get(client_req* req);

enum attr_parser {
    TABLE,
    COLUMN,
    VALUE
};

struct table_attribute {
    char* table;
    char* column;
    char* value;

    int table_size;
    int column_size;
    int value_size;
};

#endif