# Tests.
How to make them work?
1) To make perl TAP tests work (its 001-whole_test.pl), you just need Perl interpreter

2) To make python load tests work (its 002-load_test.py and 004-load_test.py), you just need python interpreter

3) To make go load tests work(its 003-load_test_.go, 005-load_test_.go, 006-load_test_.go), you just need go compiler


## about tests

1) 001-whole_test.pl - tap tests, checks that the program is working correctly
2) 002-load_test.py - creates 100 connections, each of which does connect, set, get, del, disconnect in a loop
3) 003-load_test_.go - creates 100 connections, each of which does connect, set, get, del, disconnect in a loop
4) 004-load_test.py - creates 100 connections, each of which does connect, set, get, del, disconnect  and wait one second in a loop 
5) 005-load_test_.go - creates 100 connections, each of which does set, get, del in a loop, and do connect and disconnect
   not in a loop (take a count threads, default 100)
6) 006-load_test_.go - The load test takes 2 arguments: the first is one of the commands to be applied (currently available are get, set, del), and the number of threads that will send requests.

   1) get - performs a set operation outside the loop and then, based on this set, performs get operations in a loop for 60 seconds, counting the number of operations.
   2) set - performs set operations in a loop with the same value for 60 seconds, counting the number of operations.
   3) del - performs del operations in a loop for the same value for 60 seconds, counting the number of operations.

connection and disconnection not in while