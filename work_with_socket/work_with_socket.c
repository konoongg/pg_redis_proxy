#include <unistd.h>
#include "utils/elog.h"

#include "work_with_socket.h"


int
write_data(int fd, char* mes, int count_sum){
    int writeBytes = 0;
    int cur_write_bytes = 0;
    while(cur_write_bytes != count_sum){
        writeBytes = write(fd, mes + cur_write_bytes, count_sum - cur_write_bytes);
        if(writeBytes == -1){
            char* err = strerror(errno);
            ereport(ERROR, errmsg("write err: %s", err));
            return -1;
        }
    }
    return 0;
}
