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
    event_data* r_data;
    event_data* w_data;
    connection* conn = (connection*)wcalloc(sizeof(connection));

    conn->fd = fd;

    w_data = (event_data*)wcalloc(sizeof(event_data));
    r_data = (event_data*)wcalloc(sizeof(event_data));

    r_data->handle = wcalloc(sizeof(handle));
    w_data->handle = wcalloc(sizeof(handle));

    conn->w_data = w_data;
    conn->r_data = r_data;

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

void init_wthread(wthread* wthrd) {
    wthrd->active = wcalloc(sizeof(conn_list));
    wthrd->active->first = wthrd.active->last = NULL;
    wthrd->active_size = 0;
    wthrd->wait = wcalloc(sizeof(conn_list));
    wthrd->wait->first = wthrd.wait->last = NULL;
    wthrd->wait_size = 0;
}


void loop_step(wthread* wthrd) {
    while (wthrd->active_size != 0) {
        connection* cur_conn = wthrd->active->first;
        while (cur_conn != NULL) {
            assert(!cur_conn->is_wait);
            proc_status status = cur_conn->proc(cur_conn);
            switch(status) {
                case WAIT_PROC:
                    delete_active(cur_conn);
                    add_wait(cur_conn);
                    break;
                case DEL_PROC:
                    delete_active(cur_conn);
                    finish_connection(cur_conn);
                    break;
                case ALIVE_PROC:
                    break;
            }
            cur_conn = cur_conn->next;
        }
    }
}

void free_wthread(void) {
    close(wthrd.listen_socket);
    close(wthrd.efd);
    free(wthrd.active);
    free(wthrd.wait);
    loop_destroy(wthrd.l);
}

void finish_connection(connection* conn) {
    event_stop(conn->wthrd, conn->r_data->handle);
    event_stop(conn->wthrd, conn->w_data->handle);
    free_connection(conn);
}
