#ifndef STRINGS_H
#define STRINGS_H

typedef struct string string;
int do_del (client_req* req, char** answer);
int do_set (client_req* req, char** answer);
int do_get (client_req* req, char** answer);


struct string {
    char* str;
    int size;
};

#endif