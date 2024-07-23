#!/usr/bin/python3

import socket
import threading
import time

def worker(host, port, thread_num):
    try:
        start_time = time.time()
        name = "test_" + str(thread_num)
        delete = ["del", name]
        set = ["set", name , name]
        get = ["get", name]
        set_req = "*" + str(len(set))
        delete_req = "*" + str(len(delete))
        get_req = "*" + str(len(get))
        for part in delete:
            delete_req += "\r\n$" + str(len(part)) + "\r\n" + part
        delete_req += "\r\n"
        for part in set:
            set_req += "\r\n$" + str(len(part)) + "\r\n" + part
        set_req += "\r\n"
        for part in get:
            get_req += "\r\n$" + str(len(part)) + "\r\n" + part
        get_req += "\r\n"
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((host, port))
        s.send(delete_req.encode())
        s.close()
        # print(f"suc connection to{host}:{port}")
        while(time.time() - start_time < 60):
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(5)
            s.connect((host, port))
            s.send(set_req.encode())
            result = s.recv(8192)
            if(result !=  b'+OK\r\n'):
                print(f"err wait $3\r\nset\r\n but have {result}")
            s.send(get_req.encode())
            result = s.recv(8192)
            expected = f'${len(name)}\r\n{name}\r\n'.encode('utf-8')
            if(result !=  expected):
                print(f"err wait $3\r\nset\r\n but have {result}")
            s.send(delete_req.encode())
            result = s.recv(8192)
            if(result !=  b':1\r\n'):
                print(f"err wait $3\r\nset\r\n but have {result}")
            time.sleep(1)
            s.close()
    except Exception as e:
        print(f"error connection to {host}:{port}: {e}")

def main():
    print("START")
    host = 'localhost'
    port = 6379
    num_connections = 100
    threads = []
    for i in range(num_connections):
        thread = threading.Thread(target=worker, args=(host, port, i))
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()
    print("suc test")

if __name__ == "__main__":
    main()
