# Tests.
How to make them work?
To make perl TAP tests work (its 001-whole_test.pl), you just need Perl interpreter

To make C++ TAP work (002-parser_tests.cpp) you need to:

1) Install libtap++ from Nikolay Shaplovs repo (https://gitlab.com/dhyannataraj/libtappp)
2) Probably, move the libtap++.so to correct directory (to the same directory where .so files are stored)
3) THE MOST IMPORTANT: you need to comment/remove unneeded imports in redis_reqv_parser.c
