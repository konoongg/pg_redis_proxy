package main

import (
	"fmt"
	"net"
	"sync"
	"time"
	"bytes"
	"strconv"
)

func send(conn net.Conn, req, answer []byte) bool {
    _, err := conn.Write(req)
    if err != nil {
        fmt.Printf("Failed to send request: %v\n", err)
        conn.Close()
        return false
    }
    response := make([]byte, 1024)
    res, err := conn.Read(response)
    if err != nil {
        fmt.Printf("Failed to read response: %v\n",  err)
        conn.Close()
        return false
    }
    if !bytes.Equal(response[:res], answer){
        fmt.Printf("Failed, expected: %s, but got: %s\n", answer, response[:res])
        return false
    }
    return true
}

func prepare_db(req []byte) bool{
    conn, err := net.Dial("tcp", "localhost:6379")
    _, err = conn.Write(req)
    if err != nil {
        fmt.Printf("Failed to send request: %v\n", err)
        conn.Close()
        return false
    }
    response := make([]byte, 1024)
    _, err = conn.Read(response)
    if err != nil {
        fmt.Printf("Failed to read response: %v\n",  err)
        conn.Close()
        return false
    }
    conn.Close()
    return true
}

func createReq(parts []string) string {
    req := "*" + strconv.Itoa(len(parts))
    for _, part := range parts {
        req += "\r\n$" + strconv.Itoa(len(part)) + "\r\n" + part
    }
    req += "\r\n"
    return req
}

func main(){
    var wg sync.WaitGroup
    count_threads := 100
    time_wait := 60
    count_work := make([]int, count_threads)
    for  i := 0; i < count_threads; i++ {
        wg.Add(1)
        go func(threadID int) {
            defer wg.Done()
            name := "test_" + strconv.Itoa(threadID)
            delete := []string{"del", name}
            set := []string{"set", name, name}
            get := []string{"get", name}
            delReq := createReq(delete)
            setReq :=  createReq(set)
            getReq :=  createReq(get)
            if prepare_db([]byte(delReq)) == false {
                fmt.Printf("Thread %d  can't prepare db\n")
                return;
            }
            timeout := time.After(time.Duration(time_wait) * time.Second)
            for {
                select {
                    case <-timeout:
			 // fmt.Printf("Thread %d: Timed out after %d seconds\n", threadID, time_wait)
                        return
                    default:
                        conn, err := net.Dial("tcp", "localhost:6379")
                        if err != nil {
                            fmt.Printf("Thread %d: Connection failed: %v\n", threadID, err)
                            return
                        }
                        if send(conn, []byte(setReq), []byte("+OK\r\n")) == false{
                            return
                        }
                        //expected :="$" + strconv.Itoa(4) + "\r\n" + "test" + "\r\n"
                        expected := "$" + strconv.Itoa(len(name)) + "\r\n" + name + "\r\n"
                        if send(conn, []byte(getReq), []byte(expected)) == false{
                            return
                        }
                        if send(conn, []byte(delReq), []byte(":1\r\n")) == false{
                            return
                        }
                        conn.Close()
                        count_work[threadID]++
                }
            }
       	}(i)
    }
    wg.Wait()
    all_count_work := 0
    for _, work := range count_work {
        all_count_work += work
    }
    fmt.Println("All threads have finished")
    fmt.Printf("result %d treads work: %d for %d s\n", count_threads, all_count_work, time_wait)
}
