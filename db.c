#include <stdio.h>
#include <unistd.h>

#include "postgres.h"
#include "utils/elog.h"
#include "libpq-fe.h"

#include "alloc.h"
#include "config.h"
#include "connection.h"
#include "db.h"
#include "storage_data.h"

char* create_conn_req(void);
char* create_t_info_req(char* table_name);
void connect_to_db(backend* backends);
void init_meta_data(void);

extern config_redis config;
db_meta_data* meta;

void finish_connects(backend* backends) {
    for (int i = 0; i < config.db_conf.count_backend; ++i) {
        PQfinish(backends[i].conn_with_db);
    }
}

db_oper_res write_to_db(PGconn* conn, char* req) {
    if (PQconnectPoll(conn) == PGRES_POLLING_WRITING) {
        return WAIT_OPER_RES;
    } else if (PQconnectPoll(conn) == PGRES_POLLING_FAILED) {
        return ERR_OPER_RES;
    } else if (PQconnectPoll(conn) == PGRES_POLLING_OK) {
        if (PQsendQuery(conn, req) != 1) {
            return ERR_OPER_RES;
        }
        return WRITE_OPER_RES;
    }
    return ERR_OPER_RES;
}

db_oper_res read_from_db(PGconn* conn, char* t, req_table** req) {
    if (PQconnectPoll(conn) == PGRES_POLLING_WRITING) {
        return WAIT_OPER_RES;
    } else if (PQconnectPoll(conn) == PGRES_POLLING_FAILED) {
        return ERR_OPER_RES;
    } else if (PQconnectPoll(conn) == PGRES_POLLING_OK) {
        PGresult* res = PQgetResult(conn);
        *req = create_req_by_pg(res, t);

        PQclear(res);

        res = PQgetResult(conn);
        if (res != NULL) {
            ereport(INFO, errmsg("read_from_db: second res - error"));
            return ERR_OPER_RES;
        }

        if (*req == NULL) {
            ereport(INFO, errmsg("read_from_db: can't get value"));
            return ERR_OPER_RES;
        }

        return READ_OPER_RES;
    }
    return ERR_OPER_RES;
}

char* create_conn_req(void) {
    int conn_info_size;
    char* conn_info;

    conn_info_size = CONN_INFO_DEFAULT_SIZE + strlen(config.db_conf.dbname) + strlen(getlogin());
    conn_info = wcalloc(conn_info_size * sizeof(char));
    if (sprintf(conn_info, "user=%s dbname=%s host=localhost", getlogin(), config.db_conf.dbname) < 0) {
        ereport(INFO, errmsg("create_conn_req: can't create connection info"));
        abort();
    }
    return conn_info;
}

char* create_t_info_req(char* table_name) {
    int req_size;
    char* req;
    char* table_info;

    req_size = strlen(table_name) + TABLE_INFO_SIZE;
    req = wcalloc(req_size * sizeof(char));
    table_info = "SELECT column_name, data_type, is_nullable FROM information_schema.columns WHERE table_name = '%s'";

    if (sprintf(req, table_info, table_name) < 0) {
        ereport(INFO, errmsg("create_t_info_req: can't create req"));
        abort();
    }

    return req;
}

void connect_to_db(backend* backends) {
    char* conn_info = create_conn_req();

    for (int i = 0; i < config.db_conf.count_backend; ++i) {
        backends[i].conn_with_db = PQconnectStart(conn_info);
        if (backends[i].conn_with_db == NULL) {
            ereport(INFO, errmsg("connect_to_db: PQstatus is bad - %s",  PQerrorMessage(backends[i].conn_with_db)));
            finish_connects(backends);
            abort();
        }

        if (PQstatus(backends[i].conn_with_db) == CONNECTION_BAD) {
            ereport(INFO, errmsg("connect_to_db: PQstatus is bad - %s",  PQerrorMessage(backends[i].conn_with_db)));
            finish_connects(backends);
            abort();
        }
        backends[i].fd = PQsocket(backends[i].conn_with_db);
    }

   free(conn_info);

}

column* get_column_info(char* table_name, char* column_name) {
    for (int i = 0; i < meta->count_tables; ++i) {
        table* t  = &(meta->tables[i]);
        if (strncmp(t->name, table_name, strlen(table_name)) == 0 && strlen(table_name) == strlen(t->name)) {
            for (int j = 0; j < t->count_column; ++j) {
                column* c = &(t->columns[j]);
                if (strncmp(c->column_name, column_name, strlen(column_name)) == 0 &&
                    strlen(column_name) == strlen(c->column_name)) {
                    return c;
                }
            }
        }
    }
    return NULL;
}


void init_db(backend* back) {
    init_meta_data();
    connect_to_db(back);
}

void init_meta_data(void) {
    PGresult* res;
    PGconn* conn;
    char* conn_info;
    const char* query;

    conn_info = create_conn_req();
    conn = PQconnectdb(conn_info);
    free(conn_info);

    meta = wcalloc(sizeof(db_meta_data));

    query = "SELECT tablename FROM pg_tables WHERE schemaname = 'public';";
    res = PQexec(conn, query);
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
        query = create_t_info_req(t->name);
        res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            ereport(INFO, errmsg("SELECT failed: %s", PQerrorMessage(conn)));
            PQclear(res);
            PQfinish(conn);
            abort();
        }

        t->count_column = PQntuples(res);
        t->columns = wcalloc(t->count_column  * sizeof(column));
        for (int c = 0; c < t->count_column; ++c) {
            char* column_name = PQgetvalue(res, c, 0);
            char* is_nullable = PQgetvalue(res, c, 2);
            char* type = PQgetvalue(res, c, 1);
            int column_name_size = PQgetlength(res, c, 0);

            t->columns[c].column_name = wcalloc(column_name_size * sizeof(char));
            memcpy(t->columns[c].column_name, column_name, column_name_size );

            ereport(INFO, errmsg("init_meta_data:  type: %s is_nullable: %s", type, is_nullable));

        }
        PQclear(res);
    }
    PQfinish(conn);
}
