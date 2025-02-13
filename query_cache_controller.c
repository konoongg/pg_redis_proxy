#include "errno.h"
#include <eventfd.h>
#include <pthread.h>

#include "postgres.h"
#include "utils/elog.h"

#include "alloc.h"
#include "connection.h"
#include "db.h"
#include "hash.h"
#include "query_cache_controller.h"

db_worker dbw;
extern config_redis config;

void free_command(command_to_db* cmd) {
    free(cmd->key);
    free(cmd->table);
    free(cmd->cmd);
    free(cmd);
}

void register_command(char* tabl, char* req, connection* conn, com_reason reason, char* key, int key_size) {
    command_to_db* cmd = wcalloc(sizeof(command_to_db));
    cmd->next = NULL;
    cmd->conn = conn;
    cmd->table = tabl;
    cmd->reason = reason;
    cmd->cmd = req;

    cmd->key = wcalloc(key_size * sizeof(char));
    memcpy(cmd->key, key, key_size);
    cmd->key_size = key_size;

    int err = pthread_spin_lock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    if (dbw.commands->first == NULL) {
        dbw.commands->first = dbw.commands->last = cmd;
    } else {
        dbw.commands->last->next = cmd;
        dbw.commands->last = dbw.commands->last->next;
    }
    dbw.commands->count_commands++;

    event_notify(dbw.wthrd->efd);

    err = pthread_spin_unlock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_unlock %s", strerror(err)));
        abort();
    }
}

command_to_db* get_command(void) {
    int err = pthread_spin_lock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_lock %s", strerror(err)));
        abort();
    }

    command_to_db* cmd =  dbw.commands->first;
    dbw.commands->first = dbw.commands->first->next;
    dbw.commands->count_commands--;

    if (dbw.commands->count_commands == 0) {
        dbw.commands->first = dbw.commands->last = NULL;
    }

    int err = pthread_spin_unlock(dbw.lock);
    if (err != 0) {
        ereport(INFO, errmsg("register_command: pthread_spin_unlock %s", strerror(err)));
        abort();
    }

    return cmd;
}

proc_status process_write_db(connection* conn) {
    backend* back = (backend*)conn->data;
    command_to_db* cmd = conn->w_data->data;
    db_oper_res res = write_to_db(back->conn_with_db, cmd->cmd);
    if (res == WRITE_OPER_RES) {
        conn->proc = process_read_db;
        conn->status = READ_DB;
        move_from_active_to_wait(conn);
        return WAIT_PROC;
    } else if (res == WAIT_OPER_RES) {
        move_from_active_to_wait(conn);
        return WAIT_PROC;
    } else  if (res == ERR_OPER_RES) {
        free_connection(dbw.backends);
        abort();
    }
}

proc_status process_read_db(connection* conn) {
    backend* back = (backend*)conn->data;
    command_to_db* cmd = conn->w_data->data;

    req_table* req;
    db_oper_res res = read_from_db(back->conn_with_db, cmd->table, &req);
    if (res == READ_OPER_RES) {
        back->is_free = true;
        move_from_active_to_wait(conn);
        move_from_wait_to_active(cmd->conn);

        command_to_db* cmd = conn->w_data->data;
        event_notify(cmd->conn->wthrd->efd);

        if (cmd->reason == CACHE_UPDATE) {
            char* key = cmd->key;
            int key_size = cmd->key_size;
            cache_data* data = init_cache_data(key, key_size, req);
            set_cache(data);
            free_cache_data(data);
        }

        free_command(cmd);

        int err = pthread_spin_lock(dbw.lock);
        if (err != 0) {
            ereport(INFO, errmsg("process_read_db: pthread_spin_lock %s", strerror(err)));
            abort();
        }

        event_notify(conn->wthrd->efd);

        int err = pthread_spin_unlock(dbw.lock);
        if (err != 0) {
            ereport(INFO, errmsg("process_read_db: pthread_spin_unlock %s", strerror(err)));
            abort();
        }

        return WAIT_PROC;
    } else if (res == WAIT_OPER_RES) {
        move_from_active_to_wait(conn);
        return WAIT_PROC;
    } else  if (res == ERR_OPER_RES) {
        free_connection(dbw.backends);
        abort();
    }
}

proc_status notify_db(connection* conn) {
    char code;
    int res = read(conn->fd, &code, 1);

    if (res < 0 && res != EAGAIN) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("notify: read error %s", err));
        abort();
    } else if (res == 0 || res == EAGAIN) {
        return ALIVE_PROC;
    }

    for (int i = 0; i < dbw.count_backends; ++i) {
        if (dbw.backends[i].is_free) {
            dbw.backends[i].is_free = false;
            move_from_wait_to_active(dbw.backends[i].conn);
            dbw.backends[i].conn->w_data = get_command();
            dbw.backends[i].conn->data = &(dbw.backends[i]);
            dbw.backends[i].conn->proc = process_write_db;
            dbw.backends[i].conn->status = WRITE_DB;

            start_event(dbw.wthrd->l, dbw.backends[i].conn->w_data->handle);
            break;
        }
    }

    move_from_active_to_wait(conn);
    return WAIT_PROC;
}

cache_data* init_cache_data(char* key, int key_size, req_table* args) {
    cache_data* data = wcalloc(sizeof(cache_data));
    data->key = wcalloc(key_size * sizeof(char));
    data->key_size = key_size;
    data->values = wcalloc(sizeof(values));
    data->values->count_attr = args->count_args;
    data->values->attr = wcalloc(args->count_args * sizeof(attr));
    for (int i = 0; i < args->count_args; ++i) {
        int column_name_Size = strlen(args->args[i].column_name);
        column* c = get_column_info(args->table, args->args[i].column_name);
        req_column* attr = &(data->values->attr[i]);
        attr->type = c->type;
        memcpy(attr->data, args->attr[i].data, args->attr[i].data_size);
        attr->column_name = wcalloc(column_name_Size * sizeof(char));
        memcpy(attr->column_name, args->args[i].column_name, column_name_Size);
    }
    return data;
}

void free_cache_data(cache_data* data) {
    free(data);
}

void* start_db_worker(void*) {
    while (true) {
        CHECK_FOR_INTERRUPTS();
        loop_run(dbw.wthrd->l);
        loop_step(dbw.wthrd);
    }
}

void init_db_worker(void) {
    connection* efd_conn;
    dbw.count_backends = config.db_conf.count_backend;
    dbw.backends = wcalloc(dbw.count_backends * sizeof(backend));
    init_db(dbw.backends);

    dbw.commands = wcalloc(sizeof(list_command));
    dbw.commands->first = dbw.commands->last = NULL;

    dbw.wthrd = wcalloc(sizeof(wthread));
    init_wthread(dbw.wthrd);
    dbw.wthrd->l = init_loop();

    dbw.wthrd->efd = eventfd(0, EFD_NONBLOCK);
    if (wthrd.efd == -1) {
        char* err = strerror(errno);
        ereport(INFO, errmsg("start_db_worker: eventfd error %s", err));
        abort();
    }

    efd_conn = create_connection(dbw.wthr->efd, dbw.wthrd);
    init_event(efd_conn, listen_efd_connconn->r_data->handle, efd_conn->fd, EVENT_READ);
    add_wait(efd_conn);
    efd_conn->proc = notify_db;
    efd_conn->status = NOTIFY_DB;


    for (int i = 0; i < dbw.count_backends; ++i) {
        connection* db_conn = create_connection(dbw.backends[i].fd, dbw.wthrd);
        init_event(db_conn, db_conn->r_data->handle, db_conn->fd, EVENT_READ);
        init_event(db_conn, db_conn->w_data->handle, db_conn->fd, EVENT_WRITE);
        add_wait(db_conn);
        dbw.backends[i].conn = db_conn;
    }

    pthread_t db_tid;
    int err = pthread_create(&(db_tid), NULL, start_db_worker, NULL);
    if (err) {
        ereport(INFO, errmsg("init_worker: pthread_create error %s", strerror(err)));
        abort();
    }
}
