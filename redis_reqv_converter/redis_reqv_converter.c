#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include <string.h>
#include <stdlib.h>

#include "redis_reqv_converter.h"

/*
 * Processing part (Redis command to PostgreSQL query)
 * ["get", "key", "value"] =>
 * "SELECT value FROM pg_redis_table WHERE key=key"
 *
 * Warning: totally untested.
 * TODO: put all of the processing into different files.
 */
void
process_get(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_get: %s", command_argv[0])); // plug
}

// should return +OK (or smth like that) if it worked
void
process_set(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_get: %s", command_argv[0])); // plug
}

// TODO: improve it.
void
process_command(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_command")); // plug
    // or better: this should return something like "$3\r\n(all commands supported)\r\n"
}

void
process_ping(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_ping")); // plug
    // should return smth like "+PONG" probably
}

void
to_big_case(char* string) {
    for (int i = 0; i < strlen(string); ++i) {
        if (string[i] >= 'a' && string[i] <= 'z')
            string[i] = string[i] + ('A' - 'a');
    }
}

/*
 * High-level function which redirects processing of command arguments to
 * basic cases ("get", "set", etc.)
 * TODO: all commands. Or as many commands as possible
 */
void
process_redis_to_postgres(int command_argc, char** command_argv) {
    if (command_argc == 0) {
        return; // nothing to process to db
    }
    ereport(LOG, errmsg("PROCESSING STARTED"));

    to_big_case(command_argv[0]); // converting to upper, since commands are in upper case

    if (!strcmp(command_argv[0], "GET")) {
        ereport(LOG, errmsg("GET_PROCESSING: %s", command_argv[0]));

    } else if (!strcmp(command_argv[0], "SET")) {
        ereport(LOG, errmsg("SET_PROCESSING: %s", command_argv[0]));

    } else if (!strcmp(command_argv[0], "COMMAND")) {
        ereport(LOG, errmsg("COMMAND_PROCESSING: %s", command_argv[0]));

    } else { // command not found "exception"
        ereport(LOG, errmsg("COMMAND NOT FOUND: %s", command_argv[0]));
    }
}
