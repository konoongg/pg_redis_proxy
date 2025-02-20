#include <stdlib.h>

#include "postgres.h"
#include "utils/elog.h"

#include "io.h"

void answer_free(void* data) {

    ereport(INFO, errmsg("answer_free: "));
    answer_list* a_list = (answer_list*)data;
    answer* cur_answer;
    cur_answer = a_list->first;
    while(cur_answer != NULL) {
        answer* next_answer = cur_answer->next;
        free_answer(cur_answer);
        cur_answer = next_answer;
    }
    //free(a_list);
}

void io_read_free(void* data) {
    ereport(INFO, errmsg("io_read_free: data %p", data));
    client_req* cur_req;
    io_read* r_data = (io_read*)data;

    cur_req = r_data->reqs->first;
    while(cur_req != NULL) {
        client_req* req = cur_req->next;
        free_cl_req(cur_req);
        cur_req = req;
    }

    ereport(INFO, errmsg("io_read_free: r_data->pars.parsing_str %p", r_data->pars.parsing_str));
    free(r_data->pars.parsing_str);
    ereport(INFO, errmsg("io_read_free: r_data->reqs %p", r_data->reqs));
    free(r_data->reqs);
    ereport(INFO, errmsg("io_read_free: read_buffer %p", r_data->read_buffer));
    free(r_data->read_buffer);
    // ereport(INFO, errmsg("io_read_free: r_data %p", r_data));
    // free(r_data);

    ereport(INFO, errmsg("io_read_free: FINISH"));
}