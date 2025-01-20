#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "cache.h"
#include "connection.h"
#include "resp_creater.h"
#include "strings.h"

string* init_string(int size, char* data);
void free_string(void* str);

string* init_string(int size, char* data) {
    string* str =  wcalloc(sizeof(string));
    str->size = size;
    str->str  = wcalloc(size * sizeof(char));

    memcpy(str->str, data, str->size);

    //ereport(INFO, errmsg("init_string: FINISH %p", str));
    return str;
}

void free_string(void* data) {
    string* str = (string*)data;
    free(str->str);
    free(str);
}

int do_del (client_req* req, answer* answ) {
    int count_del = 0;
    if (req->argc != 2) {
        create_err_resp(answ, "ERR syntax error");
        return 0;
    }


    //ereport(INFO, errmsg("do_del: req->argc %d", req->argc));
    for (int i = 1; i < req->argc; ++i) {
        if (lock_cache_basket(0, req->argv[i], req->argv_size[i]) != 0) {
            create_err_resp(answ, "ERR syntax error");
            return -1;
        }
        //ereport(INFO, errmsg("do_del: req->argv[%d]:%s req->argv_size[%d]:%d ", i, req->argv[i], i, req->argv_size[i]));
        count_del += free_cache_key(0, req->argv[i], req->argv_size[i]);
        if (unlock_cache_basket(0, req->argv[1], req->argv_size[1]) != 0) {
            create_err_resp(answ, "ERR syntax error");
            return -1;
        }
    }
    create_num_resp(answ, count_del);
    return 0;
}

int do_set (client_req* req, answer* answ) {
    //ereport(INFO, errmsg("do_set: TEST"));
    string* str;

    if (req->argc != 3) {
        create_err_resp(answ, "ERR syntax error");
        return 0;
    }

    str = init_string(req->argv_size[2], req->argv[2]);
    if (lock_cache_basket(0, req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return -1;
    }

    //ereport(INFO, errmsg("do_set: have lock"));


    //ereport(INFO, errmsg("do_set:  str %p", str));
    if (set_cache(0, create_data(req->argv[1], req->argv_size[1], str, STRING, free_string)) == 0) {
        create_simple_string_resp(answ, "OK");
    } else {
        create_err_resp(answ, "ERR syntax error");
    }

    if (unlock_cache_basket(0, req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return -1;
    }
    //ereport(INFO, errmsg("do_set: FINISH"));
    return 0;
}

int do_get (client_req* req, answer* answ) {

    //ereport(INFO, errmsg("do_get: TEST"));
    cache_get_result res;
    string* str ;

    if (req->argc != 2) {
        create_err_resp(answ, "ERR syntax error");
        return 0;
    }

    if (lock_cache_basket(0, req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return -1;
    }

    res = get_cache(0,  create_data(req->argv[1], req->argv_size[1], NULL, STRING, NULL));
    if (res.err == false) {
        str = res.result;
        //ereport(INFO, errmsg("do_get: create_bulk_string_resp(answ, str->str:%s, str->size:%d);", str->str, str->size));
        create_bulk_string_resp(answ, str->str, str->size);
    } else {
        create_err_resp(answ, res.err_mes);
    }

    if (unlock_cache_basket(0, req->argv[1], req->argv_size[1]) != 0) {
        create_err_resp(answ, "ERR syntax error");
        return -1;
    }

    return 0;
}
