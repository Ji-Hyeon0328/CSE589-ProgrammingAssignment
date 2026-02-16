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
    int talk_fd = socket(AF_INET,SOCK_STREAM,0);
    if(talk_fd == -1){
        perror("socket");
        exit(1);
    }


    // TODO: Populate sockaddr_in with server IP/port.
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port=htons(port_long);
    //"127.0.0.1" <-> argv[1], 
    //run command: ./client <IP> <Port> for argv[1] case
    if(inet_pton(AF_INET,argv[1],&addr.sin_addr)!=1){
        fprintf(stderr,"Invalid address: %s\n", argv[1]);
        close(talk_fd);
        exit(1);
    }

    // TODO: Connect to the server.
    if(connect(talk_fd, (struct sockaddr*)&addr, sizeof(addr))== -1){
        perror("connect");
        close(talk_fd);
        exit(1);
    }

    // TODO: Read from stdin in a loop (read()) and send in chunks.
    char buf[5000];
    while(1){
        ssize_t n = read(STDIN_FILENO,buf,sizeof(buf));
        if(n==0) break;
                
        if(n<0){
            if (errno==EINTR) continue;
            perror("read");
            close(talk_fd);
            exit(1);
        }

        // TODO: For each chunk, send the *exact bytes* you read.
        //   - Use send()/write() in a loop to handle partial sends.
        //   - Do NOT add newlines, prefixes, or other formatting.
        // The test harness compares server stdout byte-for-byte with stdin input.
        ssize_t total_sent = 0;
        while(total_sent<n){
            ssize_t sent = send(talk_fd,buf+total_sent,(n-total_sent),0);

            if(sent<0){
                if(errno==EINTR) continue;
                perror("send");
                close(talk_fd);
                exit(1);
            }

            total_sent += sent;
        }

    }
    // TODO: Close the socket before exiting.
    close(talk_fd);

    return 0;
}
