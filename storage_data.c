#include <stdlib.h>

#include "storage_data.h"

values* create_copy_data(values* v) {
    values* new_v = wcalloc(sizeof(values));
    new_v->count_attr = v->count_attr;
    new_v->attr = wcalloc(new_v->count_attr * sizeof(attr));

    for (int i = 0; i < new_v->count_attr; ++i) {
        new_v->attr[i].type = v->attr[i].type;
        new_v->attr[i].data = wcalloc(sizeof(db_data));
        memcpy(new_v->attr[i].data, v->attr[i].data, sizeof(db_data));
    }
    return new_v;
}

void free_values(values* v) {
    for (int i = 0; i < new_v->count_attr; ++i) {
        free(new_v->attr[i].data);
    }
    free(v->attr);
    free(v);
}
