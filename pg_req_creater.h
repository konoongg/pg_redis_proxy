#ifndef PG_REQ_H
#define PG_REQ_H

#include "connection.h"
#include "db.h"

#define SELECT_BASE_SIZE 28
#define SET_BASE_SIZE 56
#define DELETE_BASE_SIZE 23
#define TRANSACTION_SIZE 12

typedef enum attr_parser attr_parser;
typedef struct bd_req_attr  bd_req_attr;

char* create_pg_del(int count, char** keys, int* keys_size);
char* create_pg_get(char* key, int key_Size);

enum attr_parser {
    TABLE,
    COLUMN,
    VALUE,
};

struct bd_req_attr {
    char* table;
    char* column;
    char* value;

    int table_size;
    int column_size;
    int value_size;
};

#endif