#pragma once

#define DEFAULT_PORT            (6379) // 6379 is a default redis port
#define DEFAULT_BACKLOG_SIZE    (512)

int init_redis_listener(void);
