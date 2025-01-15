#ifndef STRINGS_H
#define STRINGS_H

#include "connection.h"

typedef struct string string;

int do_del (client_req* req, answer* answ);
int do_set (client_req* req, answer* answ);
int do_get (client_req* req, answer* answ);

struct string {
    char* str;
    int size;
};

#endif