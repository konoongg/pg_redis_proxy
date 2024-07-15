#pragma once

#define DEFAULT_PORT            (6379)
#define DEFAULT_BACKLOG_SIZE    (512)

int init_redis_listener(void);
