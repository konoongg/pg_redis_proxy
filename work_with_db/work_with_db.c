#include "libpq-fe.h"
#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>

#include "work_with_db.h"
#include "req_result.h"
#include "../configure_proxy/configure_proxy.h"

PGconn* conn;
PGresult* res = NULL;
bool connected = false;
char* transaction = NULL;
int cur_position = 0;
size_t full_transaction_size;

//Initializes the creation of a transaction, allocates the necessary memory space for the transaction.
int init_transaction(size_t transaction_size){
    full_transaction_size = transaction_size + TRANSACTION_REQV_SIZE  + 1; // add \0;
    transaction = (char*) malloc(full_transaction_size * sizeof(char));
    memset(transaction, 0, full_transaction_size);
    ereport(DEBUG1, errmsg ("transaction_size: %ld full_transaction_size: %ld", transaction_size, full_transaction_size));
    if(transaction == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return -1;
    }
    memcpy(transaction, "BEGIN;", BEGIN_REQV_SIZE);
    cur_position = BEGIN_REQV_SIZE;
    return 0;
}

/*
 * Adds a set command to the transaction for the key 'key' from the table named table_name,
 * with the transaction size anticipating a trailing semicolon at the end,
 * as the transaction may contain multiple operations.
 */
int add_set(int param_size, char* key, char* value, char* table_name){
    size_t reqv_size = UPDATE_REQV_SIZE + param_size;
    char* UPDATE = (char*)malloc((reqv_size + 1) * sizeof(char)); // snprintf add  \0
    ereport(DEBUG1, (errmsg("start set operation: key: %s value: %s table_name: %s param_size:%d reqv_size: %ld", key, value, table_name, param_size, reqv_size)));
    if(UPDATE == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return -1;
    }
    if (snprintf (UPDATE, reqv_size + 1, "UPDATE %s SET h['%s']='%s';", table_name, key, value) != reqv_size) {
        ereport (ERROR, errmsg ("sprintf err"));
        PQclear(res);
        PQfinish(conn);
        free(UPDATE);
        free_transaction();
        return -1;
    }
    ereport(DEBUG1, errmsg ("start UPDATE: %ld cur_position: %d", reqv_size, cur_position));
    memcpy(transaction + cur_position, UPDATE, reqv_size);
    ereport(DEBUG1, errmsg ("finish UPDATE"));
    cur_position += reqv_size ; //snprintf add \0 in the end DELETE and sql don't work with \0
    free(UPDATE);
    ereport(DEBUG1, (errmsg("finish set ")));
    return 0;
}

/*
 * Adds a delete command to the transaction for the key 'key' from the table named table_name,
 * with the transaction size anticipating a trailing semicolon at the end,
 * as the transaction may contain multiple operations.
 */
int add_del(int param_size, char* key, char* table_name){
    size_t reqv_size = DELETE_REQV_SIZE + param_size;
    char* DELETE = (char*)malloc((reqv_size + 1) * sizeof(char)); // snprintf add  \0
    ereport(DEBUG1, (errmsg("start del operation: key:%s table_name:%s param_size:%d", key, table_name, param_size)));
    if(DELETE == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return -1;
    }
    if (snprintf(DELETE, reqv_size + 1, "UPDATE %s SET h = delete(h, '%s');", table_name, key) != reqv_size) {
        ereport(ERROR, errmsg ("sprintf err"));
        free(DELETE);
        free_transaction();
        PQclear(res);
        PQfinish(conn);
        return -1;
    }
    ereport(DEBUG1, errmsg ("start DELETE: %ld cur_position: %d", reqv_size, cur_position));
    memcpy(transaction + cur_position, DELETE, reqv_size);
    cur_position += reqv_size;
    free(DELETE);
    ereport(DEBUG1, (errmsg("finish del ")));
    return 0;
}

//Adds a specific operation to the transaction based on the op_name.
// param_size - it is size key, value, table_name
int add_op_in_transaction(operation_name op_name, int param_size, char* key, char* value, char* table_name){
    if(transaction == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return -1;
    }
    ereport(DEBUG1, (errmsg("start add operation")));
    if(op_name == SET){
        return add_set(param_size, key, value, table_name);
    }
    else if(op_name == DEL){
        return add_del(param_size, key, table_name);
    }
    else{
        ereport(ERROR, (errmsg("uncorrected operation %d", op_name)));
        return -1;
    }
}
//Finalizes the transaction by appending a COMMIT; statement and sends it for execution.
req_result do_transaction(void){
    if(!connected){
        ereport(ERROR, errmsg("do_transaction: not connected with db"));
        return ERR_REQ;
    }
    memcpy(transaction + cur_position, "COMMIT;", COMMIT_REQV_SIZE);
    transaction[full_transaction_size - 1] = '\0';
    ereport(DEBUG1, (errmsg("transaction: %s : %ld ", transaction, full_transaction_size)));
    ereport(LOG, (errmsg("start do transaction ")));
    res = PQexec(conn, transaction);
    ereport(LOG, (errmsg("finish transaction")));
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("Setting element in hstore failed: %s", PQerrorMessage(conn)));
        free_transaction();
        return finish_abnormally();
    }
    return OK;
}

//Frees the memory allocated for the transaction.
void free_transaction(void){
    ereport(DEBUG1, (errmsg("free transaction: %s", transaction)));
    cur_position = 0;
    free(transaction);
    transaction = NULL;
}

// this function tries to connect to postgres database as current user
int init_work_with_db(void){
    char connect[200];
    if (sprintf(connect, "user=%s dbname=postgres", getlogin()) < 0) {
        ereport(ERROR, errmsg( "sprintf err"));
        PQfinish(conn);
        return -1;
    }
    ereport(INFO, errmsg("start work with db"));
    conn = PQconnectdb(connect);
    if (PQstatus(conn) == CONNECTION_BAD) {
        ereport(ERROR, errmsg( "Connection to database failed: %s", PQerrorMessage(conn)));
        PQfinish(conn);
        return -1;
    }
    connected = true;
    return 0;
}

/*
 * get_value and set_value functions execute SQL queries to get/set data
 * key stores redis-like key, char** value is a pointer to result string (value from db)
 * and length should store length of value + 1 (for \0)
*/
req_result get_value(char* table, char* key, char** value, int* length){
    int select_reqv_size = SELCT_REQV_SIZE + strlen(key) + strlen(table);
    char* SELECT;
    int n_rows;
    if(!connected){
        ereport(ERROR, errmsg("get_value: not connected with db"));
        return ERR_REQ;
    }
    SELECT = (char*)malloc((select_reqv_size + 1) * sizeof(char)); // snprintf add \0;
    if(SELECT == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return ERR_REQ;
    }
    //ereport(DEBUG1, errmsg("SELECT: %s", table));
    if (snprintf(SELECT, select_reqv_size + 1, "SELECT h['%s'] FROM %s;", key, table) != select_reqv_size) {
        ereport(ERROR, errmsg( "snprintf err select"));
        PQclear(res);
        PQfinish(conn);
        free(SELECT);
        return ERR_REQ;
    }
    res = PQexec(conn, SELECT); // execution
    //ereport(DEBUG1, errmsg("select send %s", SELECT));
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("select table failed: %s", PQerrorMessage(conn)));
        free(SELECT);
        return finish_abnormally();
    }
    n_rows = PQntuples(res);
    //ereport(DEBUG1, errmsg("COUNT ROWS %d count column: %d", n_rows, PQnfields(res)));
    *value = PQgetvalue(res, 0, 0);
    *length = PQgetlength(res, 0, 0) + 1; // + \0
    if(n_rows > 1){
        ereport(ERROR, errmsg("get_value: more than one value"));
        free(SELECT);
        return ERR_REQ;
    }
    else if(strcmp(*value, "") == 0){
        *value = NULL;
        ereport(DEBUG1, errmsg("return non"));
        free(SELECT);
        return NON;

    }
    *value = PQgetvalue(res, 0, 0);
    *length = PQgetlength(res, 0, 0) + 1; // + \0
    ereport(DEBUG1, errmsg("answer:%c  %d size: %d", PQgetvalue(res, 0, 0)[0], PQgetvalue(res, 0, 0)[0], PQntuples(res)));
    ereport(DEBUG1, errmsg("return ok"));
    free(SELECT);
    return OK;
}

req_result set_value(char* table, char* key, char* value){
    char* INSERT;
    int insert_reqv_size = UPDATE_REQV_SIZE + strlen(table) + strlen(key) + strlen(value);
    if(!connected){
        ereport(ERROR, errmsg("set_value: not connected with bd"));
        return ERR_REQ;
    }
    INSERT = (char*)malloc((insert_reqv_size + 1) * sizeof(char));// snprintf add \0;
    if(INSERT == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return ERR_REQ;
    }
    //ereport(DEBUG1, errmsg("INSERT: %s", table));
    if (snprintf (INSERT, insert_reqv_size + 1, "UPDATE %s SET h['%s']='%s';", table, key, value) != insert_reqv_size) {
        ereport (ERROR, errmsg ("sprintf err"));
        free(INSERT);
        return finish_abnormally();
    }

    res = PQexec(conn, INSERT);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("Setting element in hstore failed: %s", PQerrorMessage(conn)));
        free(INSERT);
        return finish_abnormally();
    }
    free(INSERT);
    return OK;
}

// deletes one key from table
// should return NON if no keys were deleted
req_result del_value(char* table, char* key){
    int size_del_reqv = DELETE_REQV_SIZE + strlen(key) + strlen(table);
    int size_find_reqv = FIND_REQV_SIZE + strlen(key) + strlen(table);
    char* DELETE = NULL;
    char* FIND = NULL;
    int n_rows;
    //ereport(DEBUG1, errmsg("del_value: entered function"));
    if (!connected) {
        ereport(ERROR, errmsg("del_value: not connected with db"));
        return ERR_REQ;
    }
    DELETE = (char*)malloc((size_del_reqv + 1) * sizeof(char));// snprintf add \0;
    if(DELETE == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return finish_abnormally();
    }
    // ===== Filling FIND && DELETE with needed requests =====
    if (snprintf(DELETE, size_del_reqv + 1, "UPDATE %s SET h = delete(h, '%s');", table, key) != size_del_reqv) {
        ereport(ERROR, errmsg ("snprintf err DELETE"));
        free(DELETE);
        return finish_abnormally();
    }
    FIND = (char*)malloc((size_find_reqv + 1) * sizeof(char));// snprintf add \0;
    if(FIND == NULL){
        free(DELETE);
        ereport(ERROR, (errmsg("can't malloc")));
        return finish_abnormally();
    }

    //ereport(DEBUG1, errmsg("Various info: table: %s, key: %s, request: SELECT exist(h, '%s') FROM %s", table, key, key, table));
    if (snprintf(FIND, size_find_reqv + 1, "SELECT exist(h, '%s') FROM %s;", key, table) != size_find_reqv) {
        ereport(ERROR, errmsg("snpritnf err FIND"));
        free(DELETE);
        free(FIND);
        return finish_abnormally();
    }

    // ===== Filling ended, execution starts =====
    res = PQexec(conn, FIND);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport (ERROR, errmsg("FINDING ELEMENT IN HSTORE FAILED"));
        free(DELETE);
        free(FIND);
        return finish_abnormally();
    }
    n_rows = PQntuples(res);
    if (n_rows != 1) {
        ereport(ERROR, errmsg("Incorrect result of exist(h, 'key') command"));
        free(DELETE);
        free(FIND);
        return finish_abnormally();
    }
    
    // command exist(h, "key") returns "f" if there's no element in hstore,
    // and "t" there is elelment in hstore.
    if (!strcmp(PQgetvalue(res, 0, 0), "f")){
        free(DELETE);
        free(FIND);
        return NON;
    }

    res = PQexec(conn, DELETE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport (ERROR, errmsg("Deleting element in hstore failed: %s", PQerrorMessage(conn)));
        free(DELETE);
        free(FIND);
        return finish_abnormally();
    }
    //ereport(DEBUG1, errmsg("Deletion finished"));
    free(DELETE);
    free(FIND);
    return OK;
}

req_result get_table_name(char*** tables_name, int* n_rows){
    char GET_TABLES[] = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'";
    if(!connected){
        ereport(ERROR, errmsg("set_value: not connected with bd"));
        return ERR_REQ;
    }
    res = PQexec(conn, GET_TABLES);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        ereport(ERROR, errmsg("Query execution failed: %s", PQerrorMessage(conn)));
        return finish_abnormally();
    }
    ereport(LOG, errmsg( "GET_TABLES"));
    *n_rows = PQntuples(res);
    (*tables_name) = (char**)malloc(*n_rows * sizeof(char*) );
    for(int i = 0; i < *n_rows; ++i){
        (*tables_name)[i] = PQgetvalue(res, i, 0);
    }
    return OK;
}

// creates table in PostgreSQL database.
// h is a key-value structure for strings
req_result create_table(char* new_table_name){
    int table_name_size = strlen(new_table_name);
    int create_t_size = (table_name_size + CREATE_TABLE_REQV_SIZE);
    int create_h_size = (table_name_size + CREATE_HSTORE_REQV_SIZE);
    char* CREATE_TABLE = (char*)malloc((create_t_size + 1) * sizeof(char)); // snprintf add \0
    char* CREATE_HSTORE;
    if(CREATE_TABLE == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        return ERR_REQ;
    }
    CREATE_HSTORE = (char*)malloc((create_h_size + 1) * sizeof(char)); // snprintf add \0;
    if(CREATE_HSTORE == NULL){
        ereport(ERROR, (errmsg("can't malloc")));
        free(CREATE_TABLE);
        return ERR_REQ;
    }
    // creating table
    //ereport(DEBUG1 , errmsg( "non db %s - create", new_table_name));
    if (snprintf(CREATE_TABLE, create_t_size + 1, "CREATE TABLE %s (h hstore);", new_table_name) != create_t_size) {
        ereport(ERROR, errmsg( "sprintf err"));
        free(CREATE_TABLE);
        free(CREATE_HSTORE);
        return finish_abnormally();
    }
    res = PQexec(conn, CREATE_TABLE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport(ERROR, errmsg("creating table failed: %s", PQerrorMessage(conn)));
        free(CREATE_TABLE);
        free(CREATE_HSTORE);
        return finish_abnormally();
    }

    // putting hstore into it
    if (snprintf(CREATE_HSTORE, create_h_size + 1, "INSERT INTO %s VALUES ('');", new_table_name) != create_h_size) {
        ereport(ERROR, errmsg("sprintf err"));
        free(CREATE_TABLE);
        free(CREATE_HSTORE);
        return finish_abnormally();
    }
    res = PQexec(conn, CREATE_HSTORE);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        ereport (ERROR, errmsg("creating hstore failed: %s", PQerrorMessage(conn)));
        free(CREATE_TABLE);
        free(CREATE_HSTORE);
        return finish_abnormally();
    }
    free(CREATE_TABLE);
    free(CREATE_HSTORE);
    return OK;
}

void finish_work_with_db(void){
    connected = false;
    ereport(LOG, errmsg("finish work with db"));
    PQclear(res);
    PQfinish(conn);
}

inline req_result finish_abnormally() {
    ereport(ERROR, errmsg("Finished wirk with db abnormally"));
    // PQclear(NULL) is an OK operation, it does nothing and leaves.
    PQclear(res);
    PQfinish(conn);
    return ERR_REQ;
}
