# Client-Server Application
_This project was created as a part of CS F372: OS Assignment 2_ <br /><br />
A POSIX-C code (will not run on Windows terminal) that can register multiple clients and perform simple mathematical tasks using a single server. <br />
It uses read-write lock to register multiple clients and run them in parallel. <br />
Shared memory is read and written to store and access data during runtime. <br />
SIGINT (Ctrl+C) terminates and clears current shared memory. <br />

## Compile server code
```
gcc A2_25_server.c -o A2_25_server.out -lpthread -lrt
```

## Run server (only one instance)
```
./A2_25_server.out
```

## Compile client code
```
gcc A2_25_client.c -o A2_25_client.out -lpthread -lrt
```

## Run client (pone or more instances)
```
./A2_25_client.out
```
<br />
Note: "client_test.c" and "server_test.c" are not used and were created only for testing.
