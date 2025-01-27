package main

import (
	"bytes"
	"database/sql"
	"fmt"
	"io"
	"net"
	"os"
	"time"

	_ "github.com/lib/pq"
)

const (
	ColorGreen = "\033[32m"
	ColorRed   = "\033[31m"
	ColorReset = "\033[0m"
)

const (
	host   = "localhost" // pg host
	port   = 5432        // pg port
	dbname = "postgres"  // db name
)

const (
	simpleCreateQuery = `
		CREATE TABLE IF NOT EXISTS test (
			test1 TEXT,
			test2 TEXT,
			test3 TEXT
		);`

	simpleDropQuery = `DROP TABLE IF EXISTS test;`
)

type Test struct {
	test_name   string
	callback    func(test *Test) bool
	wait_answer []byte
	real_answer []byte
}

type TestConn struct {
	connToPG    *sql.DB
	connToCache net.Conn
}

func (conn *TestConn) Close() error {
	err := conn.connToCache.Close()
	if err != nil {
		return err
	}

	_, err = conn.connToPG.Exec(simpleDropQuery)
	if err != nil {
		return fmt.Errorf("error postgres drop exec: %w", err)
	}

	err = conn.connToPG.Close()

	if err != nil {
		return err
	}

	return nil
}

func (conn *TestConn) Open() error {
	var err error

	conn.connToCache, err = net.Dial("tcp", "localhost:6379")
	if err != nil {
		return fmt.Errorf("error connect to localhost:6379: %w", err)
	}

	conn.connToCache.SetReadDeadline(time.Now().Add(2 * time.Second))

	psqlInfo := fmt.Sprintf("host=%s port=%d user=%s dbname=%s sslmode=disable",
		host, port, os.Getenv("USER"), dbname)

	conn.connToPG, err = sql.Open("postgres", psqlInfo)
	if err != nil {
		return fmt.Errorf("error connect to postgres: %w", err)
	}

	_, err = conn.connToPG.Exec(simpleCreateQuery)
	if err != nil {
		return fmt.Errorf("error postgres create exec: %w", err)
	}

	return nil
}

func simpleSetTest(test *Test) bool {
	var conn TestConn
	var err error
	test.wait_answer = []byte("+OK\r\n")

	err = conn.Open()
	if err != nil {
		fmt.Println(err.Error())
		return false
	}

	request := []byte("*3\r\n$3\r\nset\r\n$26\r\ntest.test1.simple_set_test\r\n$37\r\ntest1:simple_set_test.test2:2.test3:3\r\n")
	res := DoTest(conn.connToCache, test, request)
	err = conn.Close()
	if err != nil {
		return false
	}
	return res
}

func simpleGetTest(test *Test) bool {
	var conn TestConn
	var err error

	err = conn.Open()
	if err != nil {
		return false
	}
	defer conn.Close()

	request := []byte("*3\r\n$3\r\nset\r\n$15\r\nsimple_get_test\r\n$15\r\nsimple_get_test\r\n")
	test.wait_answer = []byte("+OK\r\n")

	if DoTest(conn.connToCache, test, request) == false {
		return false
	}

	test.wait_answer = []byte("$15\r\nsimple_get_test\r\n")
	request = []byte("*2\r\n$3\r\nget\r\n$15\r\nsimple_get_test\r\n")
	res := DoTest(conn.connToCache, test, request)

	return res
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

	tests := []Test{
		{
			test_name: "simple set test",
			callback:  simpleSetTest,
		},
		// {
		// 	test_name: "simple get test",
		// 	callback:  simpleGetTest,
		// },
	}

	for test_num, test := range tests {
		result := test.callback(&test)
		if result {
			fmt.Printf("%s%d) %s - OK%s\n", ColorGreen, test_num+1, test.test_name, ColorReset)
		} else {
			fmt.Printf("%s%d) %s - ERR: wait answer is '%s', but real answer '%s'%s\n",
				ColorRed, test_num+1, test.test_name, escapeSpecialChars(test.wait_answer),
				escapeSpecialChars(test.real_answer), ColorReset)
		}
	}
}
