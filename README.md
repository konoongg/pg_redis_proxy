# pg_redis_proxy
A redis proxy extension for postgreSQL database.

## Installation
Install dependecies:
1) libev
2) libpq
3) + modules for running Perl TAP tests, if they are not installed

Copy this repo into `$YOUR_PATH_TO_POSTGRES/contrib` folder,
then "remake" project:

```
make install
make install -C $YOUR_PATH_TO_POSTGERS/contrib/hstore
make install -C $YOUR_PATH_TO_POSTGRES/contrib/pg_redis_proxy
```

## Using pg_redis_proxy
Type this in psql console:
`CREATE EXTENSION pg_redis_proxy;`
After executing this command, a proxy is initiated. 
If your Redis server has a configured config, 
place it in the folder with postgresql.
conf, and the proxy will automatically configure itself 
according to this configuration. Currently, 
the supported config parameters are: 
port, tcp-backlog, daemonize, databases, bind, logfile, save, 
port, tcp-backlog, databases. 
The proxy supports the following Redis commands: GET, SET, DEL, PING.


The proxy supports 4 caching modes, and direct work with PostgreSQL tables is not recommended:
1) NO_CACHE: No caching, the slowest option but the most reliable
2) GET_CACHE: All del, set queries are immediately synchronized with the database
3) ONLY_CACHE: No synchronization with the database, the fastest option but the least reliable
4) DEFFER_DUMP: Specifies the synchronization time with the 
database and the maximum number of operations.
If a sufficient number of operations have accumulated or the time has elapsed,
synchronization of the cache with the database occurs.

To use caching on this proxy, you need to set up redis.conf (this file must be located in your
database folder. 

## Installation of FreeBSD
This proxy doesn't contain any linux-only modules, for this reason this proxy is also runnable on FreeBSD

To install this proxy on FreeBSD follow these steps:
1) install dependencies: perl, go, python3, p5-test2-suite, p5-ipc-run, postgresql-libpqxx, readline
2) Create new role in your database: (the following code may not be secure)
CREATE ROLE %username% SUPERUSER;!!!!
ALTER LOGIN!!!!
3) clone this repo;
4) copy files of this repo into postgres/contrib/pg_redis_proxy
5) run server
6) connect to server, run CREATE EXTENSION pg_redis_proxy;
