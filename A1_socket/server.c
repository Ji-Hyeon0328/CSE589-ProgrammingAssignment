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
    fprintf(stderr, "Usage: %s <listen-port>\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    char *end = NULL;
    long port_long = strtol(argv[1], &end, 10);
    if (!end || *end != '\0' || port_long <= 0 || port_long > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    // TODO: Create a TCP listen socket (AF_INET, SOCK_STREAM).
    // TODO: Set SO_REUSEADDR on the listen socket.
    // TODO: Bind the socket to INADDR_ANY and the given port.
    // TODO: Listen with a small backlog (e.g., 5-10).

    // TODO: Accept clients in an infinite loop.
    //   - For each client, read in chunks until EOF.
    //   - Write received bytes to stdout (handle partial writes).
    //   - Close the client socket.
    // TODO: Handle EINTR and other error cases as specified.

    // TODO: Close the listen socket before exiting.

    printf("This is the server skeleton. Fill in the TODOs.\n");
    return 0;
}
