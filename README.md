# pg_redis_proxy
A redis proxy extension for postgreSQL database.

## Installation
Copy this repo into `$YOUR_PATH_TO_POSTGRES/contrib` folder,
then "remake" project:

```
make install
make install -C $YOUR_PATH_TO_POSTGRES/contrib/pg_redis_proxy
```

## Using pg_redis_proxy
Type this in psql console:
`CREATE EXTENSION pg_redis_proxy;`
After executing this command, a proxy becomes initiated and it starts listening on the port 6379
The port accepts redis commands(1), converts them into SQL queries(2), PostgreSQL executes the commands and returns the result(3), 
proxy converts result so it fits RESP protocol(4), returns result to the user(5).

## Stages of development:
1) done, but untested and probably bugged
2) only frame partly done; "accepts" only 4 commands for now: get, set, command, ping
3) not done
4) not done
5) not done
