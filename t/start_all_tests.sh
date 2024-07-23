clear
echo "tap-tests:"
./001_whole_test.pl
echo "load-test python: speed test"
./002-load_test.py
echo "load-test go: speed test"
go run 003-load_test_.go
echo "load-test python: many connection test"
./004-load_test.py
