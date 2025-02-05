#include <stdio.h>
#include <unistd.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "config.h"
#include "db.h"
#include "storage_data.h"

extern config_redis config;

PGconn* conn = NULL;
db_meta_data* meta;

void connect_to_db() {
    int conn_info_size;
    char* conn_info;

    conn_info_size = CONN_INFO_DEFAULT_SIZE + strlen(config.db_conf.dbname) + strlen(getlogin());
    conn_info = wcalloc(conn_info_size * sizeof(char));
    if (sprintf(conn_info, "user=%s dbname=%s host=localhost", getlogin(), config.db_conf.dbname) < 0) {
        ereport(INFO, errmsg("init_db: can't create connection info"));
        abort();
    }
    conn = PQconnectdb(conn_info);
    if (conn == NULL) {
        ereport(INFO, errmsg("connect_to_db: PQstatus is bad - %s",  PQerrorMessage(conn)));
        abort();
    }

    if (PQstatus(conn) == CONNECTION_BAD) {
        ereport(INFO, errmsg("connect_to_db: PQstatus is bad - %s",  PQerrorMessage(conn)));
        PQfinish(conn);
        abort();
    }

}

column* get_column_info(char* table_name, char* column_name) {
    for (int i = 0; i < meta->count_tables; ++i) {
        table* t  = &(meta->tables[i]);
        if (strncmp(t->name, table_name, strlen(table_name)) == 0 && strlen(table_name) == strlen(t->name)) {
            for (int j = 0; j < t->count_column; ++j) {
                column* c = &(t->columns[j]);
                if (strncmp(c->name, column_name, strlen(column_name)) == 0 && strlen(column_name) == strlen(c->name)) {
                    return c
                }
            }
        }
    }
    return NULL;
}

void init_meta_data() {
    meta = wcalloc(sizeof(db_meta_data));
    const char* query = "SELECT tablename FROM pg_tables WHERE schemaname = 'public';";
    PGresult* res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(INFO, errmsg("SELECT failed: %s", PQerrorMessage(conn)));
        PQclear(res);
        PQfinish(conn);
        abort();
    }

    meta->count_tables = PQntuples(res);
    meta->tables = wcalloc(meta->count_tables * sizeof(table));
    for (int i = 0; i < meta->count_tables; ++i) {
        char* table_name = PQgetvalue(res, i, 0);
        int name_size = PQgetlength(res, i, 0);
        memcpy(meta->tables[i].name, table_name, name_size);
    }
    PQclear(res);

    for (int i = 0; i < meta->count_tables; ++i) {
        table* t = &(meta->tables[i]);
        char* pattern = "SELECT column_name, data_type, is_nullable FROM information_schema.columns WHERE table_name = '%s'";


        char* query = COLUMN_INFO_SIZE + strlen(t->name);
        if (sprintf(query, pattern, t->name) < 0) {
            ereport(INFO, errmsg("init_meta_data: can't create connection info"));
            abort();
        }

        res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            ereport(INFO, errmsg("SELECT failed: %s", PQerrorMessage(conn)));
            PQclear(res);
            PQfinish(conn);
            abort();
        }

        t->count_column = PQnfields(res);
        t->columns = wcalloc(t->count_column * sizeof(column));
    }
}

void init_db(void) {
    connect_to_db();
    init_meta_data();
}


// db_data* pars_db_data(PGresult* res) {
//     db_data* data = wcalloc(sizeof(db_data));
//     data->count_rows = PQntuples(res);
//     data->count_column = PQnfields(res);

//     data->tuples = wcalloc(data->count_rows * sizeof(char**));
//     data->size_value = wcalloc(data->count_rows * sizeof(int*));

//     for (int row = 0; row < data->count_rows; ++row) {
//         data->tuples[row] = wcalloc(data->count_column * sizeof(char*));
//         data->size_value[row] = wcalloc(data->count_column * sizeof(int));
//         for (int column = 0; column < data->count_column; ++column) {
//             data->tuples[row][column] = PQgetvalue(res, row, column);
//             data->size_value[row][column] = PQgetlength(res, row, column);
//         }
//     }
//     return data;
// }

