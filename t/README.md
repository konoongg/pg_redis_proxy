# Tests.
How to make them work?
1) To make perl TAP tests work (its 001-whole_test.pl), you just need Perl interpreter

2) To make python load tests work (its 002-load_test.py and 004-load_test.py), you just need python interpreter

3) To make go load tests work(its 003-load_test_.go its 005-load_test_.go 006-load_test_.go), you just need go compiler


## about tests

1) 001-whole_test.pl - Tests that check the correctness of operation

2) 002-load_test.py - Creates 100 threads, each making a request to port 6379, 
which consists of: connecting to the port, get, set, del, 
disconnecting from the port. The test outputs how many such 
sets of operations were performed within a certain period of time.

3) 003-load_test_.go [count threads] - The same test as 002-load_test.py, 
but written in Go and as the first parameter, 
the number of threads that will be created for accessing the port. 
The test outputs how many such sets of operations 
were performed within a certain period of time.

4) 004-load_test.py - The same test as 002-load_test.py, 
but with an artificial delay in each set of operations, 
aimed at checking that the port can listen to multiple connections simultaneously,
returns a message about the correctness of the test.

5) 005-load_test_.go [count threads] - The same test as 003-load_test_.go, 
but the set of operations does not include connecting to the port and 
disconnecting from the port. The test outputs how many such sets of operations 
were performed within a certain period of time.

6) 006-load_test_.go <get | set | del> [count threads] - A test aimed at observing
the performance of specific operations, each operation is applied in a loop, 
the format of the operation between applications does not change 
(for example, if it is a delete operation, a request to delete an element 
with the same key will always be sent).