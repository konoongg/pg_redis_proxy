package main

import (
	"fmt"
	"net"
	"sync"
	"time"
	"bytes"
	"strconv"
	"os"
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

func testGet(count_threads int){
    var wg sync.WaitGroup
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
            conn, err := net.Dial("tcp", "localhost:6379")
            if err != nil {
            fmt.Printf("Thread %d: Connection failed: %v\n", threadID, err)
                return
            }
            if prepare_db([]byte(delReq)) == false {
                fmt.Printf("Thread %d  can't prepare db\n")
                return;
            }
            if send(conn, []byte(setReq), []byte("+OK\r\n")) == false{
                return
            }
            timeout := time.After(time.Duration(time_wait) * time.Second)
            for {
                select {
                    case <-timeout:
                        return
                    default:
                        expected := "$" + strconv.Itoa(len(name)) + "\r\n" + name + "\r\n"
                        if send(conn, []byte(getReq), []byte(expected)) == false{
                            return
                        }
                        count_work[threadID]++
                }
            }
            if send(conn, []byte(delReq), []byte(":1\r\n")) == false{
                return
            }
            conn.Close()
       	}(i)
    }
    wg.Wait()
    all_count_work := 0
    for _, work := range count_work {
        all_count_work += work
    }
    fmt.Println("All threads have finished get test")
    fmt.Printf("result %d treads work: %d for %d s\n", count_threads, all_count_work, time_wait)
}

func testSet(count_threads int){
    var wg sync.WaitGroup
    time_wait := 60
    count_work := make([]int, count_threads)
    for  i := 0; i < count_threads; i++ {
        wg.Add(1)
        go func(threadID int) {
            defer wg.Done()
            name := "test_" + strconv.Itoa(threadID)
            delete := []string{"del", name}
            set := []string{"set", name, name}
            delReq := createReq(delete)
            setReq :=  createReq(set)
            conn, err := net.Dial("tcp", "localhost:6379")
            if err != nil {
                fmt.Printf("Thread %d: Connection failed: %v\n", threadID, err)
                return
            }
            if prepare_db([]byte(delReq)) == false {
                fmt.Printf("Thread %d  can't prepare db\n")
                return;
            }
            timeout := time.After(time.Duration(time_wait) * time.Second)
            for {
                select {
                    case <-timeout:
                        return
                    default:
                        if send(conn, []byte(setReq), []byte("+OK\r\n")) == false{
                            return
                        }
                        count_work[threadID]++
                }
            }
            conn.Close()
            if send(conn, []byte(delReq), []byte(":0\r\n")) == false{
                return
            }
       	}(i)
    }
    wg.Wait()
    all_count_work := 0
    for _, work := range count_work {
        all_count_work += work
    }
    fmt.Println("All threads have finished set test")
    fmt.Printf("result %d treads work: %d for %d s\n", count_threads, all_count_work, time_wait)
}

func testDel(count_threads int){
    var wg sync.WaitGroup
    time_wait := 60
    count_work := make([]int, count_threads)
    for  i := 0; i < count_threads; i++ {
        wg.Add(1)
        go func(threadID int) {
            defer wg.Done()
            name := "test_" + strconv.Itoa(threadID)
            delete := []string{"del", name}
            delReq := createReq(delete)
            if prepare_db([]byte(delReq)) == false {
                fmt.Printf("Thread %d  can't prepare db\n")
                return;
            }
            conn, err := net.Dial("tcp", "localhost:6379")
            if err != nil {
                fmt.Printf("Thread %d: Connection failed: %v\n", threadID, err)
                return
            }
            timeout := time.After(time.Duration(time_wait) * time.Second)
            for {
                select {
                    case <-timeout:
                        return
                    default:
                        if send(conn, []byte(delReq), []byte(":0\r\n")) == false {
                            return
                        }
                        count_work[threadID]++
                }
            }
            conn.Close()
       	}(i)
    }
    wg.Wait()
    all_count_work := 0
    for _, work := range count_work {
        all_count_work += work
    }
    fmt.Println("All threads have finished del test")
    fmt.Printf("result %d treads work: %d for %d s\n", count_threads, all_count_work, time_wait)
}

func main(){
    count_threads := 100
    if len(os.Args) >= 3{
        number, err := strconv.Atoi(os.Args[2])
        if err != nil {
            fmt.Printf("can't take args %v\n", err)
            return
        }
        count_threads = number
    } else if len(os.Args) < 2 {
        fmt.Printf("need more args\n")
        return
    }
    if os.Args[1] == "get" {
        testGet(count_threads);
    } else if os.Args[1] == "set" {
        testSet(count_threads);
    } else if os.Args[1] == "del" {
        testDel(count_threads);
    }
}
