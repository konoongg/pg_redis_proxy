#!/bin/perl

use Test2::V0;
use IO::Socket::INET;
use strict;

# use Socket qw(AF_INET);

# This the test file for pg_redis_proxy;
# It sends RESP queries to default redis socket (127.0.0.1:6379)
# and then tests correctness of the results.

# Connecting to the server

# Block 0: init (no tests yet)

my $socket = new IO::Socket::INET (
	PeerHost => '127.0.0.1',
	PeerPort => '6379',
	Proto => 'tcp',
) or die "couldn't connect to Redis socket $!n";

print "Connected to Redis (or Redis proxy) server\n";

my $request = "";
my $response = "";
my $length = -1;

# all tests in these hashes store test like this:
# name_test[request_to_redis_db] => redis_response
my %set_tests = (
	"*3\r\n\$3\r\nset\r\n\$3\r\nkey\r\n\$5\r\nvalue\r\n" => "+OK\r\n",
	"*4\r\n\$3\r\nset\r\n\$5\r\nabcde\r\n\$5\r\nefghi\r\n\$5\r\n12345\r\n" => "-ERR syntax error\r\n",
);

my %del_tests = (
	"*3\r\n\$3\r\ndel\r\n\$4\r\nkey1\r\n\$4\r\nkey2\r\n" => ":2\r\n",
	"*2\r\n\$3\r\ndel\r\n\$4\r\nkey3\r\n" => ":1\r\n",
);

my %other_tests = (
	"*1\r\n\$4\r\nping\r\n" => "+PONG\r\n",
	"*2\r\n\$4\r\nping\r\n\$3\r\naaa\r\n" => "\$3\r\naaa\r\n"
);

# there can be mistakes on how these errors look like in redis server; first test here just doesn't give response
my %uncorrect_resp_tests = (
	"alsdjfo4joifjodsmoive9iji9jivrj" => "-ERR illegal according to RESP",
	"\f\r\n\r" => "-ERR illegal according to RESP",
	"*get value" => "-ERR illegal according to RESP",
	"*2get value" => "-ERR illegal according to RESP",
	"*2\r\nget value" => "-ERR illegal according to RESP",
	"*2\r\n\$get value" => "-ERR illegal according to RESP",
	"*2\r\n\rget value" => "-ERR illegal according to RESP",
	"*2\r\n\$4\r\nget value" => "-ERR illegal according to RESP",
	"*2\r\n\$3\r\nget value\n" => "-ERR illegal according to RESP",
	"*2\r\n\$3\r\nget\r\n\$4\r\nvalue\r\n" => "-ERR illegal according to RESP",
	"*2\r\n\$3\r\nget\r\$5\r\nvalue\r\n" => "-ERR illegal according to RESP",
	"*1\r\n\$3\r\nget\r\n\$5value\r\n" => "-ERR illegal according to RESP",
	"*1\r\n\$3\r\fget\r\n\$5value\r\n" => "-ERR illegal according to RESP",
);

#tests with incorrect commands/incorrect arguments
my %correct_resp_tests = (
	"*1\r\n\$32\r\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n" => "-ERR unknown command `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`, with args beginning with: \r\n",
	"*1\r\n\$5\r\neidkc\r\n" => "-ERR unknown command `eidkc`, with args beginning with: \r\n",
);
####################################################################################################################################################################################
# Block 1: basic commands (get/set/del)
####################################################################################################################################################################################
# Block 1.1: set tests
####################################################################################################################################################################################

#1, 2
for (keys %set_tests) {
    $request = "*2\r\n\$3\r\nDEL\r\n\$3\r\nkey\r\n";
    $socket->send($request);
    $socket->recv($response, 1024);

	$socket->send($_);
	$socket->recv($response, 1024);
	# print("Response: $response, Cur: $_, Hash: $other_tests{$_}\n");
	ok($response eq $set_tests{$_}, "Set test");
}

####################################################################################################################################################################################
# Block 1.2: get tests
####################################################################################################################################################################################
#3
# checking its work:
$request = "*2\r\n\$3\r\nDEL\r\n\$3\r\nkey\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\nset\r\n\$3\r\nkey\r\n\$5\r\nvalue\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*2\r\n\$3\r\nget\r\n\$3\r\nkey\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq "\$5\r\nvalue\r\n", "Get test: get works");
####################################################################################################################################################################################

#4
#pasring two reqv
$request = "*2\r\n\$3\r\nDEL\r\n\$3\r\nkey\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\nset\r\n\$3\r\nkey\r\n\$5\r\nvalue\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*2\r\n\$3\r\nget\r\n\$3\r\nkey\r\n*2\r\n\$3\r\nget\r\n\$3\r\nkey\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq "\$5\r\nvalue\r\n\$5\r\nvalue\r\n", "Get test: get two response");
####################################################################################################################################################################################

#5
#del one test
$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_test\r\n\$8\r\ndel_test\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*2\r\n\$3\r\ndel\r\n\$8\r\ndel_test\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq ":1\r\n", "DEL test: del one");
####################################################################################################################################################################################

#6
#del not exist
$request = "*2\r\n\$3\r\ndel\r\n\$8\r\ndel_test\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*2\r\n\$3\r\ndel\r\n\$8\r\ndel_test\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq ":0\r\n", "DEL test: not exist");
######################################################################################################################################################################################

#7
#del two
$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_two1\r\n\$8\r\ndel_two1\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_two2\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\ndel\r\n\$8\r\ndel_two1\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq ":2\r\n", "DEL test: del two");
#########################################################################################################################################################################################

#8
#del one of two (fisrst)
$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_two1\r\n\$8\r\ndel_two1\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_two2\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*2\r\n\$3\r\ndel\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\ndel\r\n\$8\r\ndel_two1\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq ":1\r\n", "DEL test: del one of two (first)");
###########################################################################################################################################################################################

#9
#del one of two (second)
$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_two1\r\n\$8\r\ndel_two1\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\nset\r\n\$8\r\ndel_two2\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*2\r\n\$3\r\ndel\r\n\$8\r\ndel_two1\r\n";
$socket->send($request);
$socket->recv($response, 1024);

$request = "*3\r\n\$3\r\ndel\r\n\$8\r\ndel_two1\r\n\$8\r\ndel_two2\r\n";
$socket->send($request);
$socket->recv($response, 1024);
ok($response eq ":1\r\n", "DEL test: del one of two (second)");

##5
## "bad" situations
## Wrong number of arguments
#$request = "*2\r\n\$3\r\nDEL\r\n\$3\r\nkey\r\n";
#$socket->send($request);
#$socket->recv($response, 1024);
#
#$request = "*3\r\n\$3\r\nget\r\n\$3\r\nkey\r\n\$20\r\nUn3x1st1ng_k3y_11251\r\n";
#$socket->send($request);
#$socket->recv($response, 1024);
#ok($response eq "-ERR wrong number of arguments for `get` command\r\n", "Get test: incorrect number of arguments(2)");

#$request = "*1\r\n\$3\r\nget\r\n";
#$socket->send($request);
#$socket->recv($response, 1024);
#ok($response eq "-ERR wrong number of arguments for 'get' command\r\n", "Get test: incorrect number of arguments(0)");
####################################################################################################################################################################################

##6
## trying to get unexisting key:ls
#$request = "*2\r\n\$3\r\nget\r\n\$20\r\nUn3x1st1ng_k3y_11251\r\n";
#$socket->send($request);
#$socket->recv($response, 1024);
#ok($response eq "\$-1\r\n", "Get test: unexisting value");

####################################################################################################################################################################################
# Block 1.3: del tests
####################################################################################################################################################################################

#7
# del command receives any amount of arguments; keys in arguments are deleted.
# filling the table
#for (my $i = 0; $i < 4; $i++) {
#	$request = "*3\r\n\$3\r\nset\r\n\$4\r\nkey$i\r\n\$5\r\nabcdef\r\n";
#	$socket->send($request);
#	$socket->recv($response, 1024);
#}

#for (keys %del_tests) {
#	$socket->send($_);
#	$socket->recv($response, 1024);
#	ok($response eq $del_tests{$_}, "Del test: del deletes keys and returns correct count of keys deleted");
#
#	# print "Response: $response, Keys: $_";
#
#	# on repeat of deletion resp should return :0, because there's nothing to delete
#	$socket->send($_);
#	$socket->recv($response, 1024);
#	ok($response eq ":0\r\n", "Del test: on repeating, del doesn't delete keys");
#}

####################################################################################################################################################################################
# Block 2: other commands
####################################################################################################################################################################################

#8
#for (keys %other_tests) {
#	$socket->send($_);
#	$socket->recv($response, 1024);
#	# print("Response: $response, Cur: $_, Hash: $other_tests{$_}\n");
#	ok($response eq $other_tests{$_}, "Other tests");
#}

####################################################################################################################################################################################
# Block 3: sending incorrect data and checking extension for correct failure
####################################################################################################################################################################################
# Block 3.1: uncorrect according to RESP protocol
####################################################################################################################################################################################

# ATTENTION
# All of these tests fail/don't respond even on official server. For this reason, they were commented
# 

# for (keys %uncorrect_resp_tests) {
# 	$size = $socket->send($_);
# 	$socket->recv($response, 1024);
# 	# print("Response: $response, Cur: $_, Hash: $uncorrect_resp_tests{$_}\n");
# 	ok($response eq $uncorrect_resp_tests{$_});
# }

# Block 3.2: correct according to RESP protocol, but still must cause errors

#for (keys %correct_resp_tests) {
#	$socket->send($_);
#	$socket->recv($response, 1024);
#	print("Response: $response, Cur: $_, Hash: $correct_resp_tests{$_}\n");
#	ok($response eq $correct_resp_tests{$_}, "Uncorrect commands test");
#}

# Block 4: misc

# something else?

done_testing();
$socket->close();
