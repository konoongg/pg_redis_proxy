#define FRONTEND 1

#include <exception>
#include <string>
#include <cstdlib>
#include <fcntl.h>

#include "../redis_reqv_parser/redis_reqv_parser.c"

#define WANT_TEST_EXTRAS
#include <tap++/tap++.h>

using namespace TAP;

/*
 * Before using these test:
 * 1) make sure that redis_reqv_parser.c doesn't contain includes such as
 * 		"postgres.h", "utils/elog.h", etc
 * 2) make sure that CORRECT_TESTS_COUNT is equeal to amount of values in correct_tests array
*/


/*
 * input is string like *2\r\n$3\r\nget\r\n$3\r\nkey\r\n
 * argc is a real number of arguments in test
 * Warning: this testing program expects that there are no commands that use more than 8 arguments 
 * (which is probably not true, but enough for this moment of time) 
*/

const int MAXIMAL_AMOUNT_OF_ARGUMENTS = 8;
const char* const TESTING_FILENAME = "test_file.txt";


struct ParserTest {
	const char* input_data;
	int command_argc;
	const char* command_argv[MAXIMAL_AMOUNT_OF_ARGUMENTS];
} typedef ParserTest;



// tests that are supposed to work
ParserTest correct_tests[] = {
	{"*2\r\n$3\r\nget\r\n$3\r\nkey\r\n", 2, {"get", "key"}},
	{"*2\r\n$3\r\ndel\r\n$10\r\nkey1qaz2wx\r\n", 2, {"del", "key1qaz2wx"}},
	{"*2\r\n$3\r\ndEl\r\n$10\r\nKEY1qaz2wx\r\n", 2, {"dEl", "KEY1qaz2wx"}},
	{"*8\r\n$1\r\na\r\n$1\r\na\r\n$1\r\na\r\n$1\r\na\r\n$1\r\na\r\n$1\r\na\r\n$1\r\na\r\n$1\r\na\r\n", 8, {"a", "a", "a", "a", "a", "a", "a", "a"}},
	{"*0\r\n", 0, {}},
	{"*1\r\n$29\r\nidjoskmeiojdkmeifojskdiodjkcl\r\n", 1, {"idjoskmeiojdkmeifojskdiodjkcl"}},
	{"*1\r\n$5\r\n55555\r\n", 1, {"55555"}}
};

const int CORRECT_TESTS_COUNT = 7;

// tests that are supposed to fail
ParserTest incorrect_tests[] = {
	{"alsdjfo4joifjodsmoive9iji9jivrj", -1, {"-ERR illegal according to RESP"}},
	{"\f\r\n\r", -1, {"-ERR illegal according to RESP"}},
	{"*get value", -1, {"-ERR illegal according to RESP"}},
	{"*2get value", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\nget value", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$get value", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n\rget value", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$4\r\nget value", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget value\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget\r\n$4\r\nvalue\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget\r$5\r\nvalue\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*1\r\n$3\r\nget\r\n$5value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*1\r\n$3\r\fget\r\n$5\r\rvalue\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*3\r\n$3\r\fget\r\n$5\n\nvalue\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\fget\r\n$4value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget\r\n$1value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\fget\r\n$0value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\n\0get\r\n$-11value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$asdf\r\nget\r\n$asdfasvalue\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget\r\n$5value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"$2\r\n$3\r\nget\r\n$5value\r\n", -1, {"-ERR illegal according to RESP"}},
	{";2\r\n$3\r\nget\r\n$5value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"$5\r\n$3\r\nget\r\n$5value\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget\r\n$5\r\rvalue\r\n", -1, {"-ERR illegal according to RESP"}},
	{"*2\r\n$3\r\nget\r\n$5\n\nvalue\r\n", -1, {"-ERR illegal according to RESP"}},

	{"*2\r\n$3\r\nget\r\n$3\r\nkey\r\noqejeovmsdmivomveoekjflsaj;dos", -1, {"-ERR trash at the end"}},
};

const int INCORRECT_TESTS_COUNT = 25;

// this function basically puts request into a file. 
void prepare_test_file(const char* test_request) {
	int fd = open(TESTING_FILENAME, O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		perror("Preparation of test file failed");
		exit(1);
	}

	write(fd, test_request, strlen(test_request));

	close(fd);
}

void ok_wrapper(ParserTest test) {
	int command_argc = 0;
	char** command_argv = (char**)malloc(10 * sizeof(char*));
	int fd;
	int status;

	prepare_test_file(test.input_data);

	fd = open(TESTING_FILENAME, O_RDONLY);
	if (fd == -1) {
		perror("Open failed");
		return;
	}

	status = parse_cli_mes(fd, &command_argc, &command_argv);
	ok(status == 0, "Status is correct for correct request");

	status = ok(command_argc == test.command_argc, "correct amount of args");
	if (!status) {
		printf("Correct number of arguments: %d, from parser: %d\n", test.command_argc, command_argc);
		goto END_OF_TESTING;
	}

	for (int i = 0; i < test.command_argc; ++i) {
		status = ok(strcmp(command_argv[i], test.command_argv[i]) == 0, "Some argument is correct");
		if (!status) {
			printf("Correct arg: %s, arg from parser: %s\n", command_argv[i], test.command_argv[i]);
			goto END_OF_TESTING;
		}
	}

END_OF_TESTING:
	free(command_argv);
	close(fd);
	// to avoid garbage at the end situation
	remove(TESTING_FILENAME);
}

void incorrect_ok_wrapper (ParserTest test) {
	int fd;
	int status = -1;
	int command_argc = 0;
	char** command_argv = (char**)malloc(10 * sizeof(char*));

	prepare_test_file(test.input_data);

	fd = open(TESTING_FILENAME, O_RDONLY);
	if (fd == -1) {
		perror("Open failed");
		return;
	}

	status = parse_cli_mes(fd, &command_argc, &command_argv);

	status = ok(status != 0, "Status is correct for incorrect request"); // this means that status shouldn't be equal to 0

	if (!status) {
		printf("Your parser thinks that the input is correct, which is not true!\n");
		goto END_OF_TESTING;
	}

	status = ok(strcmp(command_argv[0], test.command_argv[0]) == 0, "Correct error message");
	if (!status) {
		printf("Correct error message %s, parser's error message", test.command_argv[0], command_argv[0]);
		goto END_OF_TESTING;
	}

END_OF_TESTING:
	free(command_argv);
	close(fd);
	remove(TESTING_FILENAME);
}

void test_everything() {
	printf("___---=== TESTING STARTED ===---___\n");
	printf("Testing with correct data\n\n");
	for (int i = 0; i < CORRECT_TESTS_COUNT; ++i) {
		printf("Test number %d is being run\n", i);
		ok_wrapper(correct_tests[i]);
	}

	printf("\nTesting with incorrect data\n\n");

	for (int i = 0; i < INCORRECT_TESTS_COUNT; ++i) {
		printf("Test number %d is being run\n", CORRECT_TESTS_COUNT + i);
		incorrect_ok_wrapper(incorrect_tests[i]);
	}

	printf("___---=== TESTING ENDED ===---___\n");
}

int main() {
	TEST_START(100500); // just random number of tests, don't pay attention to it.

	test_everything();

	TEST_END;
	return 0;
}
