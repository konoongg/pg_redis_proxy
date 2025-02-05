#ifndef DB_H
#define DB_H

#define CONN_INFO_DEFAULT_SIZE 28
#define COLUMN_INFO_SIZE 99

#include "libpq-fe.h"

#include "cache.h"
#include "storage_data.h"

typedef struct column column;
typedef struct table table;
typedef struct db_meta_data db_meta_data;

void init_db();
column* get_column_info(char* table_name, char* column_name);

#endif
