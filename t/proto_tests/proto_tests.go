package main

import (
	"bytes"
	"fmt"
	"io"
	"net"
	"time"
)

func SimpleSetTest(conn net.Conn, test *Test) bool {
	request := []byte("*3\r\n$3\r\nset\r\n$15\r\nsimple_set_test\r\n$15\r\nsimple_set_test\r\n")
	test.wait_answer = []byte("+OK\r\n")
	return DoTest(conn, test, request)
}

func SimpleGetTest(conn net.Conn, test *Test) bool {
	request := []byte("*3\r\n$3\r\nset\r\n$15\r\nsimple_get_test\r\n$15\r\nsimple_get_test\r\n")
	test.wait_answer = []byte("+OK\r\n")

	if DoTest(conn, test, request) == false {
		return false
	}

	test.wait_answer = []byte("$15\r\nsimple_get_test\r\n")
	request = []byte("*2\r\n$3\r\nget\r\n$15\r\nsimple_get_test\r\n")
	return DoTest(conn, test, request)
}

func SimpleDelTest(conn net.Conn, test *Test) bool {
	test.wait_answer = []byte("+OK\r\n")
	request := []byte("*3\r\n$3\r\nset\r\n$15\r\nsimple_del_test\r\n$15\r\nsimple_del_test\r\n")

	if DoTest(conn, test, request) == false {
		return false
	}

	request = []byte("*2\r\n$3\r\ndel\r\n$15\r\nsimple_del_test\r\n")
	test.wait_answer = []byte(":1\r\n")
	return DoTest(conn, test, request)
}

func DoubleSet(conn net.Conn, test *Test) bool {
	request := []byte("*3\r\n$3\r\nset\r\n$15\r\ndouble_set_test\r\n$15\r\ndouble_set_test\r\n")
	test.wait_answer = []byte("+OK\r\n")

	if DoTest(conn, test, request) == false {
		return false
	}

	return DoTest(conn, test, request)
}

const (
	ColorGreen = "\033[32m"
	ColorRed   = "\033[31m"
	ColorReset = "\033[0m"
)

type Test struct {
	test_name   string
	callback    func(conn net.Conn, test *Test) bool
	wait_answer []byte
	real_answer []byte
}

func DoTest(conn net.Conn, test *Test, request []byte) bool {
	count_write, err := conn.Write(request)
	if err != nil || count_write != len(request) {
		fmt.Println("Error sending request: ", err)
		return false
	}
	test.real_answer = make([]byte, len(test.wait_answer))

	count_read, err := io.ReadFull(conn, test.real_answer)
	if err != nil || count_read != len(test.wait_answer) {
		fmt.Println("Error sending request: ", err)
		return false
	}

	if bytes.Equal(test.real_answer, test.wait_answer) {
		return true
	}

	return false
}

func escapeSpecialChars(data []byte) string {
	result := ""
	for _, b := range data {
		switch b {
		case '\n':
			result += "\\n"
		case '\r':
			result += "\\r"
		default:
			result += string(b)
		}
	}
	return result
}

func main() {
	address := "localhost:6379"

	conn, err := net.Dial("tcp", address)
	if err != nil {
		fmt.Println("Ошибка подключения:", err)
		return
	}
	defer conn.Close()
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))

	tests := []Test{
		{
			test_name: "simple set test",
			callback:  SimpleSetTest,
		},
		{
			test_name: "simple get test",
			callback:  SimpleGetTest,
		},
		{
			test_name: "simple del test",
			callback:  SimpleDelTest,
		},
		{
			test_name: "double set",
			callback:  DoubleSet,
		},
	}

	for test_num, test := range tests {
		result := test.callback(conn, &test)
		if result {
			fmt.Printf("%s%d) %s - OK%s\n", ColorGreen, test_num+1, test.test_name, ColorReset)
		} else {
			fmt.Printf("%s%d) %s - ERR: wait answer is '%s', but real answer '%s'%s\n",
				ColorRed, test_num+1, test.test_name, escapeSpecialChars(test.wait_answer), escapeSpecialChars(test.real_answer), ColorReset)
		}
	}
}
