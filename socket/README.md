# Socket Programming Assignment

## Overview
You are writing a TCP client/server pair in C. This is a core topic in computer networking because sockets are the standard interface for sending and receiving data over a network. Even though you will test on a single machine (localhost), the same APIs are used for communication across different machines on a real network.

## Why you can test on one machine (and still use real networking)
Your client/server code works the same on a real network, and you can also test it on a single machine. Both are valid:

- Two machines: client connects to the server's real IP address.
- One machine: client connects to 127.0.0.1 (localhost).

When you use 127.0.0.1, your OS still creates a real TCP connection. The same TCP/IP stack handles connection setup, buffering, and delivery. The only difference is that the packets loop back inside your machine instead of going out over a physical network.

Think of it like this (single machine):

```
Client process                             OS TCP/IP stack                         Server process
--------------                             -----------------                       --------------
./client 127.0.0.1:12345  ->  [src=127.0.0.1:random_ephemeral -> dst=127.0.0.1:12345]  ->  ./server :12345
```

Even on one machine, the client uses its own ephemeral source port, the server listens on its own port, and the OS routes the bytes through the full TCP/IP stack.

Everything is real networking: connection setup, buffering, retransmission, and the byte stream semantics. The only difference is that the "wire" is inside your computer, so it is fast and easy to debug.

If you later run the client on another machine and point to the server's IP, the code is the same. The automated tests are single-machine for convenience, but you are welcome to test across two machines as well.

## Basic terminal concepts (very important)
You will use the terminal to run programs. Two important concepts:

1) stdin (standard input)
   - This is the input data your program reads.
   - By default, stdin comes from the keyboard.
   - When we use a pipe like:

     ```
     printf "Hello\n" | ./client 127.0.0.1 12345
     ```

     the output of printf becomes the stdin of ./client.

2) stdout (standard output)
   - This is the output your program writes.
   - By default, stdout is shown on the screen.
   - stdout can be redirected to a file, for example:

     ```
     ./server 12345 > server_output.txt
     ```

   - Our server writes received bytes to stdout, so you can see what arrived or redirect it to a file for comparison.

## What you need to implement
You are given skeleton code with TODO comments. Your job is to complete the missing core networking logic.

### Client requirements (client.c)
Make a client that reads bytes from stdin and sends them to the server reliably.

Concrete steps (plain language):
1) Create a TCP socket with `socket(AF_INET, SOCK_STREAM, 0)`.
2) Read the server IP and port from the command line (for example: `./client 127.0.0.1 12345`).
3) Build a `sockaddr_in` with that IP/port and call `connect()`.
4) In a loop, read from stdin into a buffer (for example 4KB). Stop when you hit EOF.
5) For each buffer, call `send()` in a loop until all bytes are sent. This matters because `send()` may send only part of the buffer.
6) If `read()` or `send()` returns `-1` with `errno == EINTR`, just retry the same call.
7) When stdin is done, close the socket and exit.

Expected behavior:
- The client does not print extra output.
- The bytes sent must be exactly the bytes read from stdin, in the same order.

### Server requirements (server.c)
Make a server that accepts many clients and writes all received bytes to stdout.

Concrete steps (plain language):
1) Create a TCP listening socket with `socket(AF_INET, SOCK_STREAM, 0)`.
2) Set `SO_REUSEADDR` so you can restart the server quickly.
3) Bind to `INADDR_ANY` and the port given on the command line.
4) `listen()` with a small backlog (5-10 is fine).
5) Loop forever: accept a client, then read from that client until EOF, then close that client socket.
6) As you read bytes from the client, write them to stdout. `write()` can be partial, so loop until all bytes are written.
7) If `read()` or `write()` returns `-1` with `errno == EINTR`, retry.

Expected behavior:
- The server keeps running and can handle multiple clients one after another.
- It should not add extra formatting; stdout should be exactly what clients send.

## How to build (step-by-step)
1) Open a terminal.
2) Change into this directory:

   ```
   cd socket
   ```

3) Build both programs:

   ```
   make
   ```

You should now have two executables:

```
./client
./server
```

## How to test (manual)
1) Open Terminal A and start the server:

   ```
   ./server 12345
   ```

   Keep this terminal open; the server keeps running.

2) Open Terminal B and send a message with the client:

   ```
   printf "Hello\n" | ./client 127.0.0.1 12345
   ```

3) Look at Terminal A. You should see:

   ```
   Hello
   ```

Notes:
- 127.0.0.1 means "this same machine."
- 12345 is the server's listening port.
- The client does NOT use 12345 as its own source port; the OS assigns a temporary client port automatically. The connection is identified by:
  client_ip:client_port -> server_ip:server_port
  So both sides do NOT use the same port.

## Automated test script
We provide a test script that runs 5 tests. You must pass all five tests.

Run it from this directory:

```
./test_client_server.sh 12345
```

If the port is already in use, choose another port (10000-60000).

### What each test means
1) Short text message  
   - Sends a small human-readable string (a few bytes).  
   - Tests the basic connect/send/receive path with the simplest input.

2) Long alphanumeric text payload  
   - Sends a long string of letters and numbers.  
   - Tests that your client and server correctly handle data larger than a single `read()` or `send()` call, and that partial sends/reads are handled.

3) Long binary payload  
   - Sends a long buffer containing arbitrary byte values (not just printable text).  
   - Tests that you are treating data as raw bytes, not C strings, and that you are not stopping early on `\\0` bytes.

4) Sequential short messages from separate clients  
   - Starts multiple clients one after another, each sending a short message.  
   - Tests that the server keeps running and can accept new clients after previous ones close.

5) Concurrent clients sending the same message  
   - Starts multiple clients at the same time, all sending the same data.  
   - Tests that the server handles concurrent connections correctly and does not crash or lose data.

Requirement: Your submission must pass all five tests.

## Report to submit
Submit a short report that contains the output from running the test script. The report can be the terminal output copied into a text or Markdown file. No extra analysis is required.

## Files in this folder
- client.c: skeleton with TODOs
- server.c: skeleton with TODOs
- Makefile: build client/server
- test_client_server.sh: test harness with detailed output

## Tips
- Read/write in loops; network calls can be partial.
- Check return values carefully and print errors with perror.
- Keep output exactly as received; do not add extra formatting.
