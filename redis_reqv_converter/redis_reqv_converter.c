#include "postgres.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "executor/spi.h"
#include <string.h>
#include <stdlib.h>


#include "redis_reqv_converter.h"

/*
 * Processing part (Redis command to PostgreSQL query)
 * ["get", "key", "value"] =>
 * "SELECT value FROM pg_redis_table WHERE key=key"
 *
 * All processing functions return int value. If this value is SUCCESSFUL_EXECUTION(0), then everything is ok.
 * Otherwise check redis_reqv_converter.h for exit codes
 *
 * Warning: totally untested.
 * 
 */


 /*
  * For now get works with any [key] argument (no matter if its just "array of bytes" or a normal string)
 */
int
process_get(int command_argc, char** command_argv) {
    int status;
    char* query;

    ereport(LOG, errmsg("IN process_get"));

    if (command_argc != 1) {
        ereport(LOG, errmsg("Command get recieved incorrect amount of arguments"));
        return INCORRECT_AMOUNT_OF_ARGUMENTS;
    }

    status = SPI_connect();
    ereport(LOG, errmsg(status == SPI_OK_CONNECT ? "Connected to SPI successfully" : "Couldn't connect to SPI"));
    if (status != SPI_OK_CONNECT)
        return SPI_GENERAL_ERROR;
    
    asprintf(&query, "SELECT value FROM redis_table WHERE key=%s", command_argv[0]);
    ereport(LOG, errmsg("GET_QUERY: %s", query));
    
    // execution of query

    status = SPI_execute(query, true, 1);
    switch (status) {
        case SPI_OK_SELECT:
            ereport(LOG, errmsg("Results from table was selected"));
            return SUCCESSFUL_EXECUTION;
        
        case SPI_ERROR_ARGUMENT:
            ereport(ERROR, errmsg("argument error"));
            break;
        case SPI_ERROR_COPY:
            ereport(ERROR, errmsg("Error copy"));
            break;
        case SPI_ERROR_TRANSACTION:
            ereport(ERROR, errmsg("Transaction error"));
            break; 
        case SPI_ERROR_OPUNKNOWN:
            ereport(ERROR, errmsg("Opunknown error"));
            break; 
        case SPI_ERROR_UNCONNECTED:
            ereport(ERROR, errmsg("Unconnected error"));
            break; 
        default:
            ereport(ERROR, errmsg("Something strange happened"));
            break;
    }


    ereport(LOG, errmsg(SPI_finish() == SPI_OK_FINISH ? "Closed connection to SPI successfully" : "Couldn't close the connection to SPI"));
    return SPI_GENERAL_ERROR;
    // return "+OK";
}

// should return +OK (or smth like that) if it worked
int
process_set(int command_argc, char** command_argv) {
    int status;
    char* query;

    ereport(LOG, errmsg("IN process_set"));

    if (command_argc != 2) {
        ereport(LOG, errmsg("Command set recieved incorrect amount of arguments (%d)", command_argc));
        return INCORRECT_AMOUNT_OF_ARGUMENTS;
    }

    status = SPI_connect();
    ereport(LOG, errmsg(status == SPI_OK_CONNECT ? "Connected to SPI successfully" : "Couldn't connect to SPI"));
    if (status != SPI_OK_CONNECT) {
        return SPI_GENERAL_ERROR;
    }

    
    asprintf(&query, "UPDATE redis_table SET value=%s WHERE key=%s", command_argv[0], command_argv[1]);
    ereport(LOG, errmsg("QUERY CREATED: %s", query));
    
    status = SPI_execute(query, false, 1);

    switch (status) {
        case SPI_OK_UPDATE:
            ereport(LOG, errmsg("Table was successfully updated"));
            return SUCCESSFUL_EXECUTION;
            break;
        
        case SPI_ERROR_ARGUMENT:
            ereport(ERROR, errmsg("argument error"));
            break;
        case SPI_ERROR_COPY:
            ereport(ERROR, errmsg("Error copy"));
            break;
        case SPI_ERROR_TRANSACTION:
            ereport(ERROR, errmsg("Transaction error"));
            break; 
        case SPI_ERROR_OPUNKNOWN:
            ereport(ERROR, errmsg("Opunknown error"));
            break; 
        case SPI_ERROR_UNCONNECTED:
            ereport(ERROR, errmsg("Unconnected error"));
            break; 
        default:
            ereport(ERROR, errmsg("Something strange happened"));
            break;
    }

    status = SPI_finish();
    ereport(LOG, errmsg(status == SPI_OK_FINISH ? "Closed connection to SPI successfully" : "Couldn't close the connection to SPI"));

    free(query);

    ereport(LOG, errmsg("EXITING process_set"));

    return SPI_GENERAL_ERROR;
}

int
process_command(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_command"));

    return SUCCESSFUL_EXECUTION;
}

int
process_ping(int command_argc, char** command_argv) {
    ereport(LOG, errmsg("IN process_ping"));

    return SUCCESSFUL_EXECUTION;
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
int
process_redis_to_postgres(int command_argc, char** command_argv) {
    if (command_argc == 0) 
        return NO_COMMAND; // nothing on input, nothing on output

    ereport(LOG, errmsg("PROCESSING STARTED"));

    to_big_case(command_argv[0]); // converting to upper, since commands are in upper case

    if (!strcmp(command_argv[0], "GET")) {
        ereport(LOG, errmsg("PROCESSING GET"));

        return process_get(command_argc - 1, command_argv + 1);
        
    } else if (!strcmp(command_argv[0], "SET")) {
        ereport(LOG, errmsg("PROCESSING SET"));

        return process_set(command_argc - 1, command_argv + 1);

    } else if (!strcmp(command_argv[0], "COMMAND")) {
        ereport(LOG, errmsg("PROCESSING COMMAND"));

        return process_command(command_argc - 1, command_argv + 1);

    } else if (!strcmp(command_argv[0], "PING")) {
        ereport(LOG, errmsg("PROCESSING PING"));

        return process_ping(command_argc - 1, command_argv + 1);

    } 
    else { // command not found "exception"
        ereport(LOG, errmsg("COMMAND NOT FOUND: %s", command_argv[0]));
        
        return NO_COMMAND;
    }

}
