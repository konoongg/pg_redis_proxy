#ifndef QUERY_CACHE_C
#define QUERY_CACHE_C

#include "db_type.h"

typedef struct db_meta db_meta;
typedef struct tables_basket tables_basket;
typedef struct table table;
typedef struct attr attr;

void init_db_worker(void)

struct attr {
    char* name;
    db_type type;
};

struct table {
    char* name;
    int count_attr;
    attr* a;
};

struct tables_basket {
    table* first;
    table* last;
};

struct db_meta {
    int count_table;
    tables_basket* tables;
};

#endif