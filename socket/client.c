#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <server-ip> <server-port>\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }

    char *end = NULL;
    long port_long = strtol(argv[2], &end, 10);
    if (!end || *end != '\0' || port_long <= 0 || port_long > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }

    // TODO: Create a TCP socket (AF_INET, SOCK_STREAM).
    // TODO: Populate sockaddr_in with server IP/port.
    // TODO: Connect to the server.

    // TODO: Read from stdin in a loop, sending in chunks.
    // TODO: Handle partial sends and EINTR for both read/send.

    // TODO: Close the socket before exiting.

    printf("This is the client skeleton. Fill in the TODOs.\n");
    return 0;
}
