#define FRONTEND 1

#include <exception>
#include <string>
#include <cstdlib>
#include <fcntl.h>

// #include "../redis_reqv_parser/redis_reqv_parser.c"

#include "../redis_reqv_converter/redis_reqv_converter.c"

#define WANT_TEST_EXTRAS
#include <tap++/tap++.h>
using namespace TAP;


/*
 * Tests for redis_reqv_converter.h
 * Before usage command all strings
 * 
 */

// some commands use more
struct ConverterTest {
    char* input1;
    char* input2;
    char* input3;

    char* output;
    int output_length;
} typedef ConverterTest;

ConverterTest set_tests[8] = {
    "key", "value", NULL, NULL, 6
};

ConverterTest get_tests[8] = {
    "key", NULL, NULL, NULL, 5
};


int main() {

    return EXIT_SUCCESS;
}