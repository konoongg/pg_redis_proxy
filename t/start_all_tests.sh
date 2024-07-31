clear
echo "001-whole_test.pl - tap tests, checks that the program is working correctly"
./001_whole_test.pl
echo "003-load_test_.go 4 - creates 4 connections, each of which does connect, set, get, del, disconnect in a loop"
go run 003-load_test_.go 4
echo "002-load_test.py - creates 100 connections, each of which does connect, set, get, del, disconnect in a loop"
./002-load_test.py
echo "004-load_test.py - creates 100 connections, each of which does connect, set, get, del, disconnect  and wait one second in a loopt"
./004-load_test.py
echo "005-load_test_.go 4 - creates 4 connections, each of which does set, get, del in a loop, and do connect and disconnect not in a loop (take a count threads)"
go run 005-load_test_.go 4
echo "006-load_test_.go get test 4 threads"
go run 006-load_test_.go get 4
echo "006-load_test_.go set test 4 threads"
go run 005-load_test_.go set 4
echo "006-load_test_.go del test 4 threads"
go run 005-load_test_.go del 4