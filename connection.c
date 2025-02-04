#include "connection.h"

void add(conn_list* list) ;
void delete(connection* conn, conn_list* list);

void add(conn_list* list) {
    if (list->first == NULL) {
        list->first = list->last = wcalloc(sizeof(connection));
    } else {
        list->last->next = wcalloc(sizeof(connection));
        list->last->next->prev = list->last;
        list->last = list->last->next ;
    }
}

void delete(connection* conn, conn_list* list) {
    if (conn->prev == NULL) {
        list->first = conn->next;
    } else {
        conn->prev->next = conn->next;
    }

    if (conn->next == NULL) {
        list->last = conn->prev;
    }
}

void add_active(connection* conn) {
    conn->is_wait = false;
    add(conn->wthrd->active);
    conn->wthrd->active_size++;
}

void delete_active(connection* conn) {
    delete(conn, conn->wthrd->active);
    conn->wthrd->active_size--;
}

void add_wait(connection* conn) {
    conn->is_wait = true;
    add(conn->wthrd->wait);
    conn->wthrd->wait_size++;
}

void delete_active(connection* conn) {
    delete(conn, conn->wthrd->wait);
    conn->wthrd->wait_size--;
}

connection* create_connection(int fd, wthread* wthrd) {
    read_data* r_data;
    write_data* w_data;
    char* read_buffer;
    requests* reqs;
    connection* conn = (connection*)wcalloc(sizeof(connection));

    conn->fd = fd;

    w_data = (write_data*)wcalloc(sizeof(write_data));
    r_data = (read_data*)wcalloc(sizeof(read_data));

    r_data->handle = wcalloc(sizeof(handle));
    w_data->handle = wcalloc(sizeof(handle));

    read_buffer = wcalloc(config.worker_conf.buffer_size * sizeof(char));
    reqs = wcalloc(sizeof(requests));
    reqs->first = reqs->last = NULL;
    reqs->count_req = 0;

    conn->w_data = w_data;
    w_data->answers = wcalloc(sizeof(answer_list));
    w_data->answers->first = w_data->answers->last = NULL;

    conn->r_data = r_data;
    r_data->cur_buffer_size = 0;
    r_data->pars.cur_count_argv = 0;
    r_data->pars.cur_size_str = 0;
    r_data->pars.parsing_num = 0;
    r_data->pars.parsing_str = NULL;
    r_data->pars.size_str = 0;
    r_data->read_buffer = read_buffer;
    r_data->pars.cur_read_status = ARRAY_WAIT;
    r_data->reqs = reqs;

    conn->next = NULL;
    conn->prev = NULL;
    conn->wthrd = wthrd;
    return conn;
}

void free_connection(connection* conn) {

    read_data* r_data = (read_data*)conn->r_data;
    write_data* w_data = (write_data*)conn->w_data;
    client_req* cur_req;
    answer* cur_answer;

    if (close(conn->fd) == -1) {
        char* err_msg = strerror(errno);
        ereport(INFO, errmsg("process_close: close() failed: %s\n", err_msg));
        abort();
    }

    cur_req = conn->r_data->reqs->first;
    while(cur_req != NULL) {
        client_req* req = cur_req->next;
        free_req(cur_req);
        cur_req = req;
    }

    cur_answer = conn->w_data->answers->last;
    while(cur_answer != NULL) {
        answer* next_answer = cur_answer->next;
        free_answer(cur_answer);
        cur_answer = next_answer;
    }

    free(conn->r_data->reqs);
    free(conn->r_data->pars.parsing_str);
    free(conn->r_data->read_buffer);
    free(conn->r_data->handle);

    free(conn->w_data->handle);
    free(conn->w_data->answers);

    free(conn->r_data);
    free(conn->w_data);
    free(conn);
}