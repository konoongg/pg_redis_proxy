package main

import (
	"fmt"
	"io"
	"net"
	"time"
)

type Test struct {
	test_name   byte
	callback    func(conn net.Conn, test *Test) bool
	wait_answer []byte
	real_answer []byte
}

func simple_set_test(conn net.Conn, test *Test) bool {
	test.wait_answer = "+OK\r\n"
	request := "*3\r\n$3\r\nset\r\n$5\r\nvalue\r\n"

	_, err := conn.Write([]byte(request))
	if err != nil {
		fmt.Println("Ошибка отправки запроса:", err)
		return false
	}

	buffer := make([]byte, test.wait_answer)

	// Читаем точное количество байт
	_, err = io.ReadFull(conn, buffer)
	if err != nil {
		fmt.Println("Ошибка чтения ответа:", err)
		return false
	}

	// Сохраняем реальный ответ в структуре
	test.real_answer = string(buffer)

	// Сравниваем ответ с ожидаемым
	if test.real_answer == test.wait_answer[:expectedBytes] {
		return true
	}

	return false

	return true
}

func simole_get_test(conn net.Conn, test *Test) bool {

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
			callback:  simple_set_test,
		},
	}

	for test_num, test := range tests {
		result := test.callback(conn, &test)
		if result {
			fmt.Printf("%d) %s - OK \n", test_num, test.test_name)
		} else {
			fmt.Printf("%d) %s - ERR: wait answer is '%s', but real answer '%s'\n",
				test_num, test.test_name, test.wait_answer, test.real_answer)
		}
	}
}
