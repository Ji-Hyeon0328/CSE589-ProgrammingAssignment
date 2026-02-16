#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
//https://beej.us/guide/bgnet/html/#setsockoptman
static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <listen-port>\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    char *end = NULL;
    long port_long = strtol(argv[1], &end, 10); //port #
    if (!end || *end != '\0' || port_long <= 0 || port_long > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    // TODO: Create a TCP listen socket (AF_INET, SOCK_STREAM).
    int listen_sock_fd=socket(AF_INET,SOCK_STREAM,0); //fd: File description
    if(listen_sock_fd== -1){
        perror("Generate Socket");
        exit(1);
    }

    // TODO: Set SO_REUSEADDR on the listen socket.
    int yes = 1;
    if (setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))== -1) {
        perror("setsockopt");
        exit(1);
    }



    // TODO: Bind the socket to INADDR_ANY and the given port.
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  //given IP, - netinet/in.h >> grep -R "INADDR_ANY" /usr/include/netinet/in.h
    addr.sin_port = htons(port_long);

    if (bind(listen_sock_fd, (struct sockaddr*)&addr, sizeof(addr))== -1){
        perror("Bind");
        exit(1);
    }


    // TODO: Listen with a small backlog (e.g., 5-10).
    int backlog = 8;
    if(listen(listen_sock_fd,backlog)== -1){
        perror("listen");
        exit(1);
    }


    // TODO: Accept clients in an infinite loop.
    char buf[5000];
    // struct sockaddr_in client_addr;
    // memset(&client_addr,sizeof(addr));
    while(1){
        int new_fd = accept(listen_sock_fd, NULL, NULL);
        if(new_fd == -1){
            perror("accept");
            continue; //waiting for another connection from client
        }
        //   - For each client, read in chunks until EOF.
        while(1){
            ssize_t n = recv(new_fd, buf, sizeof(buf),0);
            
            if(n<0){
                if(errno==EINTR) continue;
                perror("recv");
                break;
            }
            if(n==0) break; 
            
            //   - For each chunk, write those *exact bytes* to stdout.
            //     Use write(STDOUT_FILENO, ...) in a loop to handle partial writes.
            //   - Do NOT use printf/fputs or add separators/newlines/prefixes.
            //   - The test harness compares server stdout byte-for-byte with client input.
            ssize_t total_written=0;
            while(total_written<n){
                ssize_t wrt = write(STDOUT_FILENO,buf+total_written, (n-total_written));
                
                // TODO: Handle EINTR and other error cases as specified.
                if(wrt<0){
                    if (errno==EINTR) continue;
                    perror("write");
                    total_written =n;
                    break;
                }
                total_written +=wrt;
            }
  
        }

        // TODO: Close the listen socket before exiting.
        close(new_fd);
    }

    

    return 0;
}
