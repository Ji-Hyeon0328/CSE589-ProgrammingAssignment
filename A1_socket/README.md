# Socket Programming Assignment (Student Version)

## Overview
You are writing a TCP client/server pair in C. Sockets are the standard interface for
sending and receiving data over a network. Even though you will test on a single machine
(localhost), the same APIs are used for communication across different machines.

## Why localhost still uses real networking
Your client/server code works the same on a real network and on a single machine. Both are
valid:

- Two machines: client connects to the server's real IP address.
- One machine: client connects to 127.0.0.1 (localhost).

When you use 127.0.0.1, your OS still creates a real TCP connection. The same TCP/IP stack
handles connection setup, buffering, and delivery. The only difference is that the packets
loop back inside your machine instead of going out over a physical network.

Think of it like this (single machine):

  Client process                             OS TCP/IP stack                         Server process
  --------------                             -----------------                       --------------
  ./client 127.0.0.1:12345  ->  [src=127.0.0.1:random_ephemeral -> dst=127.0.0.1:12345]  ->  ./server :12345

Even on one machine, the client uses its own ephemeral source port, the server listens on
its own port, and the OS routes the bytes through the full TCP/IP stack.

Everything is real networking: connection setup, buffering, retransmission, and the byte
stream semantics. The only difference is that the "wire" is inside your computer, so it is
fast and easy to debug.

If you later run the client on another machine and point to the server's IP, the code is
the same. Our automated tests are single-machine for convenience, but you are welcome to
test across two machines as well.

## Basic terminal concepts (very important)
You will use the terminal to run programs. Two important concepts:

1) stdin (standard input)
   - This is the input data your program reads.
   - By default, stdin comes from the keyboard.
   - When we use a pipe like:
       printf "Hello\n" | ./client 127.0.0.1 12345
     the output of printf becomes the stdin of ./client.

2) stdout (standard output)
   - This is the output your program writes.
   - By default, stdout is shown on the screen.
   - stdout can be redirected to a file, for example:
       ./server 12345 > server_output.txt
   - Our server writes received bytes to stdout, so you can see what arrived or
     redirect it to a file for comparison.

## What you need to implement
You are given skeleton code with TODO comments. Your job is to complete the missing core
networking logic.

### Client requirements (client.c)
- Create a TCP socket.
- Parse server IP/port, build sockaddr_in, and connect.
- Read from stdin until EOF, sending data in chunks.
- Handle partial sends (send may send fewer bytes than requested).
- Handle EINTR for read/send properly.
- Close the socket and exit cleanly.

### Server requirements (server.c)
- Create a TCP listening socket.
- Set SO_REUSEADDR to allow quick restarts.
- Bind to INADDR_ANY and the given port.
- Listen with a small backlog (e.g., 5-10).
- Accept clients in a loop.
- For each client: read until EOF and write bytes to stdout.
- Handle partial writes and EINTR.
- Keep running unless terminated by an external signal.

## Exact I/O behavior required (part of the code requirements)
### Server output rules
- Output is raw bytes only. Do not add prefixes, tags, or extra newlines.
- When you receive bytes from a client into a buffer, write those same bytes to
  standard output using write(). This is a byte-for-byte pass-through.
- write(fd, buf, count) takes:
  - fd: the file descriptor to write to. Use STDOUT_FILENO for standard output.
  - buf: pointer to the buffer holding the bytes you received.
  - count: number of bytes to write from that buffer.
- Because write() can write fewer bytes than requested, you must loop until all
  bytes from the received chunk are written.
- If you want to save server output to a file, redirect stdout:
  - ./server 12345 > server_output.txt

### Client sending rules
- Read from standard input using read() (stdin is your keyboard or a pipe).
- read(fd, buf, count) takes:
  - fd: the file descriptor to read from. Use STDIN_FILENO for standard input.
  - buf: pointer to the buffer to fill with incoming bytes.
  - count: maximum number of bytes to read into that buffer.
- For each chunk read, send exactly those bytes to the server (no formatting).
- Like write(), send()/write() can send fewer bytes than requested, so you must
  loop until the entire chunk is transmitted.
- If you want to feed a file as input, you can redirect or use cat:
  - ./client 127.0.0.1 12345 < input.txt
  - cat input.txt | ./client 127.0.0.1 12345

## How to build (step-by-step)
1) Open a terminal.
2) Change into the student directory:
     cd student
3) Build both programs:
     make

You should now have two executables:
  ./client
  ./server

## How to test (manual)
1) Open Terminal A and start the server:
     ./server 12345
   Keep this terminal open; the server keeps running.

2) Open Terminal B and send a message with the client:
     printf "Hello\n" | ./client 127.0.0.1 12345

3) Look at Terminal A. You should see:
     Hello

Notes:
- 127.0.0.1 means "this same machine."
- 12345 is the server's listening port.
- The client does NOT use 12345 as its own source port; the OS assigns a temporary
  client port automatically. The connection is identified by:
    client_ip:client_port -> server_ip:server_port
  So both sides do NOT use the same port.

## Automated test script
We provide a test script that runs 5 tests:
1) Short text message
2) Long alphanumeric text payload
3) Long binary payload
4) Sequential short messages from separate clients
5) Concurrent clients sending the same message

Testing note (important)
- When running the tests, remove or disable any printf-style debugging to stdout.
  Any extra stdout output will be mixed into the byte stream and will cause test
  failures. If you need debugging during development, consider printing to stderr
  instead.

Run it from the student directory:
  ./test_client_server.sh 12345

If the port is already in use, choose another port (10000-60000).

## Files in this folder
- client.c: student skeleton with TODOs
- server.c: student skeleton with TODOs
- Makefile: build client/server
- test_client_server.sh: test harness with detailed output

## Tips
- Read/write in loops; network calls can be partial.
- Check return values carefully and print errors with perror.
- Keep output exactly as received; do not add extra formatting.
