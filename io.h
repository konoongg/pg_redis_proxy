#ifndef IO_H
#define IO_H

typedef enum exit_status exit_status;
typedef enum read_status read_status;
typedef struct answer answer;
typedef struct answer_list answer_list;
typedef struct client_req client_req;
typedef struct io_read io_read;
typedef struct parsing parsing;
typedef struct requests requests;

void answer_free(void* data);
void io_read_free(void* data);

#define free_answer(answ) \
    if (answ != NULL) { \
        free(answ->answer); \
        free(answ); \
        answ = NULL; \
    }

#define free_cl_req(req) \
    if (req != NULL) { \
        for (int i = 0; i < req->argc; ++i) { \
            free(req->argv[i]); \
        } \
        free(req->argv); \
        free(req); \
        req = NULL; \
    }


/* status finish parsing
 * NOT_ALL - Need more data, wait data and reading
 * */
enum exit_status {
    ERR = -1,
    NOT_ALL,
    ALL
};

struct client_req {
    char** argv;
    int argc;
    int* argv_size;
    struct client_req* next;
};

struct answer {
    char* answer;
    int answer_size;
    answer* next;
};

struct answer_list {
    answer* first;
    answer* last;
};

// parsers status
enum read_status {
    ARRAY_WAIT,
    ARGC_WAIT, // count argc
    NUM_WAIT,
    STRING_WAIT, // wait sym of string
    START_STRING_WAIT, //wait $
    CR, // skip \r
    LF, // WAIT \n after \r
    END // skip last \n\r
};

struct parsing {
    char* parsing_str;
    int cur_count_argv;
    int cur_size_str;
    int parsing_num;
    int size_str;
    read_status cur_read_status;
    read_status next_read_status;
};

struct requests {
    client_req* last;
    client_req* first;
    int count_req;
};

struct io_read {
    char* read_buffer;
    int buffer_size;
    int cur_buffer_size;
    parsing pars;
    requests* reqs;
};

#endif