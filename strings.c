#include <errno.h>
#include <stdlib.h>

#include "postgres.h"
#include "utils/elog.h"

#include "cache.h"
#include "connection.h"
#include "resp_creater.h"
#include "strings.h"

void* copy_string(void* src, void** dst) {
    free_string(*dst);
    string* str = (string*)src;
    *dst = malloc(sizeof(string));
    if (*dst == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("pars_data: malloc error - %s", err_msg));
        return NULL;
    }

    ((string*)*dst)->size = str->size;

    ((string*)*dst)->str = malloc(str->size * sizeof(char));
    if (((string*)*dst)->str == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("pars_data: malloc error - %s", err_msg));
        return NULL;
    }

    memcpy( ((string*)*dst)->str, str->str, str->size);
}

string* init_string(int size, char* data) {
    string* str;
    str->size = size;
    str->str  = malloc(size * sizeof(char));

    if (str->str == NULL) {
        char* err_msg = strerror(errno);
        ereport(ERROR, errmsg("pars_data: malloc error - %s", err_msg));
        return NULL;
    }

    memcpy(str->str, data, str->size);

    return str;
}

void free_string(string* str) {
    free(str->str);
    free(str);
}

int do_del (client_req* req, char** answer) {
    if (req->argc != 2) {
        create_err_resp(answer, "ERR syntax error");
        return 0;
    }

    if (lock_cache_basket(0, req->argv[1]) != 0) {
        create_err_resp(answer, "ERR syntax error");
        return -1;
    }

    cache_free_key(req->argv_size[2]);

    if (unlock_cache_basket(0, req->argv[1]) != 0) {
        create_err_resp(answer, "ERR syntax error");
        return -1;
    }
    return 0;
}

int do_set (client_req* req, char** answer) {
    if (req->argc != 3) {
        create_err_resp(answer, "ERR syntax error");
        return 0;
    }

    string* str = init_string(req->argv_size[2], req->argv[2]);
    if (str == NULL) {
        return -1;
    }

    if (lock_cache_basket(0, req->argv[1]) != 0) {
        create_err_resp(answer, "ERR syntax error");
        return -1;
    }

    if (set_cache(0, create_data(req->argv[1], req->argv_size[1], str, STRING, free_string)) == 0) {
        create_simple_string_resp(answer, "OK");
    } else {
        create_err_resp(answer, "ERR syntax error");
    }

    if (unlock_cache_basket(0, req->argv[1]) != 0) {
        create_err_resp(answer, "ERR syntax error");
        return -1;
    }

    return 0;
}

int do_get (client_req* req, char** answer) {
    if (req->argc != 2) {
        create_err_resp(answer, "ERR syntax error");
        return 0;
    }

    if (lock_cache_basket(0, req->argv[1]) != 0) {
        create_err_resp(answer, "ERR syntax error");
        return -1;
    }

    string* str ;
    if (get_cache(0,  create_data(req->argv[1], req->argv_size[1], NULL, STRING, NULL)) == 0) {
        create_bulk_string_resp(answer, str->str, str->size);
    } else {
        create_err_resp(answer, "ERR syntax error");
    }

    if (unlock_cache_basket(0, req->argv[1]) != 0) {
        create_err_resp(answer, "ERR syntax error");
        return -1;
    }

    return 0;
}
