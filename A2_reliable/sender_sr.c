#define _POSIX_C_SOURCE 200809L
#include "netif.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --listen PORT --peer_ip IP --peer_port PORT --in FILE --win N --timeout MS\n",
            prog);
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

// Creating a structure for storing packets
typedef struct {
    uint8_t packet[PKT_HDR_LEN + MAX_PAYLOAD];
    uint64_t packet_len;
    uint32_t seq;
    uint64_t timeeout;
    bool ack;
} Packet;

int main(int argc, char **argv) {
    int listen_port = -1;
    const char *peer_ip = NULL;
    int peer_port = -1;
    const char *in_path = NULL;
    int win = -1;
    int rto_ms = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--peer_ip") == 0 && i + 1 < argc) {
            peer_ip = argv[++i];
        } else if (strcmp(argv[i], "--peer_port") == 0 && i + 1 < argc) {
            peer_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--win") == 0 && i + 1 < argc) {
            win = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            rto_ms = atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    #define WINDOW_N (uint32_t)win

    if (listen_port <= 0 || !peer_ip || peer_port <= 0 || !in_path || win <= 0 || rto_ms <= 0) {
        usage(argv[0]);
        return 1;
    }

    FILE *in = fopen(in_path, "rb");
    if (!in) {
        perror("fopen");
        return 1;
    }

    struct stat st;
    if (fstat(fileno(in), &st) != 0) {
        perror("fstat");
        fclose(in);
        return 1;
    }
    uint64_t file_size = (uint64_t)st.st_size;

    // Create a UDP socket through the netif wrapper (emulator-aware).
    int sock = netif_socket();
    if (sock < 0) {
        fclose(in);
        return 1;
    }

    // Bind local port for this sender.
    if (netif_bind(sock, listen_port) != 0) {
        fclose(in);
        close(sock);
        return 1;
    }
    // Tell emulator which peer port to forward to.
    if (netif_connect(sock, peer_ip, peer_port) != 0) {
        fclose(in);
        close(sock);
        return 1;
    }

    uint8_t buf[PKT_HDR_LEN + MAX_PAYLOAD];
    uint8_t recvbuf[PKT_HDR_LEN + MAX_PAYLOAD];
    uint32_t seq = 0;
    uint64_t data_sent = 0;
    uint64_t data_retx = 0;
    uint64_t ack_rcvd = 0;
    uint64_t start_ms = 0;
    uint64_t end_ms = 0;

    // Basic sender: send each data packet once, then send FIN once.
    // TODO(student): implement GBN/SR reliability here:
    //   - keep a send window and buffer unacked packets
    //   - start/restart timers and retransmit on timeout
    //   - process ACKs to slide the window and compute RTT/RTO

    //To store n packets
    Packet window[WINDOW_N];
    int64_t window_start_idx = 0;
    size_t nread = 1;
    bool all_acked = false;
    
    while(seq < WINDOW_N && nread != 0){


        printf("Sending seq %u\n", seq);

        nread = fread(window[seq].packet + PKT_HDR_LEN, 1, MAX_PAYLOAD, in);
        if (nread == 0) {
            break;
        }

        // Build a DATA packet: header + payload.
        window[seq].seq = seq;
        size_t pktlen = pkt_build_data(window[seq].packet, sizeof(window[seq].packet), seq, window[seq].packet + PKT_HDR_LEN, (uint16_t)nread);
        if (pktlen == 0) {
            fprintf(stderr, "packet build failed\n");
            fclose(in);
            close(sock);
            return 1;
        }
        window[seq].packet_len = pktlen;
        if (netif_send(sock, window[seq].packet, window[seq].packet_len) < 0) {
            perror("sendto");
            fclose(in);
            close(sock);
            return 1;
        }
        window[seq].timeeout = now_ms() + rto_ms;
        window[seq].ack = false;
        seq++;
        data_sent += 1;
    }

    while (nread!=0 || !all_acked) {

        // Example ACK receive path (non-blocking). We only count ACKs here.
        // TODO(student): use ACKs to slide the window and retransmit on timeout.
        ssize_t rn = netif_recv(sock, recvbuf, sizeof(recvbuf), 0);
        if (rn > 0) {
            printf("Received ACK!\n");
            pkt_hdr_t hdr;
            if (pkt_parse(recvbuf, (size_t)rn, &hdr, NULL, NULL) == 0) {
                if (hdr.type == PKT_TYPE_ACK) {
                    ack_rcvd++;
                    uint32_t ack_seq = hdr.ack;
                    for (uint32_t j = 0; j < WINDOW_N; j++) {
                        if (window[j].seq == ack_seq) {
                            window[j].ack=true;
                            printf("Received ACK for seq %u\n", ack_seq);
                            break;
                        }
                    }
                }
            }
        }


        int64_t j = window_start_idx;
        bool cumul_ack = true;
        all_acked = true;
        int64_t cumul_ack_idx = -1;
        while(j < window_start_idx + WINDOW_N && j < seq ){
            int64_t window_idx = j % WINDOW_N;
            if(window[window_idx].ack){
                if(cumul_ack){
                    cumul_ack_idx = j;
                }
            }else {
                    all_acked = false;
                    cumul_ack = false;
                    if(window[window_idx].timeeout < now_ms()){
                        printf("Retransmitting because of timeout seq %u\n", window[window_idx].seq);
                        if (netif_send(sock, window[window_idx].packet, window[window_idx].packet_len) < 0) {
                            perror("sendto");
                            fclose(in);
                            close(sock);
                            return 1;
                        }
                        data_retx++;
                        window[window_idx].timeeout = now_ms() + rto_ms;
                }
            }
            j++;
        }

        if(cumul_ack_idx != -1){
            int64_t k = window_start_idx;
            window_start_idx = cumul_ack_idx + 1;
            printf("Repopulating from %ld to %ld", k, window_start_idx);
            while(k <= cumul_ack_idx){
                int64_t window_idx = k % WINDOW_N;
                nread = fread(window[window_idx].packet + PKT_HDR_LEN, 1, MAX_PAYLOAD, in);
                if (nread == 0) {
                    break;
                }
                // Build a DATA packet: header + payload.
                window[window_idx].seq = seq;
                size_t pktlen = pkt_build_data(window[window_idx].packet, sizeof(window[window_idx].packet), seq, window[window_idx].packet + PKT_HDR_LEN, (uint16_t)nread);
                if (pktlen == 0) {
                    fprintf(stderr, "packet build failed\n");
                    fclose(in);
                    close(sock);
                    return 1;
                }
                window[window_idx].packet_len = pktlen;
                if (netif_send(sock, window[window_idx].packet, window[window_idx].packet_len) < 0) {
                    perror("sendto");
                    fclose(in);
                    close(sock);
                    return 1;
                }
                printf("Sending new seq %u\n", seq);
                window[window_idx].timeeout = now_ms() + rto_ms;
                window[window_idx].ack = false;
                seq++;
                data_sent += 1;
                k++;
            }
        }

        
        if (start_ms == 0) {
            start_ms = now_ms();
        }
        
    }

    // Basic FIN send (no retransmission).
    // Build and send FIN to mark end of file.
    size_t fin_len = pkt_build_fin(buf, sizeof(buf), seq);
    if (fin_len > 0) {
        netif_send(sock, buf, fin_len);
    }

    bool finacked = false;
    int retries = 0;
    while (!finacked && retries<10) {

        // Basic wait for FINACK (no retries).
        uint64_t wait_ms = 0;
        while (wait_ms < (uint64_t)rto_ms) {
            ssize_t n = netif_recv(sock, recvbuf, sizeof(recvbuf), 50);
            if (n > 0) {
                pkt_hdr_t hdr;
                if (pkt_parse(recvbuf, (size_t)n, &hdr, NULL, NULL) == 0) {
                    if (hdr.type == PKT_TYPE_FINACK) {
                        end_ms = now_ms();
                        finacked = true;
                        break;
                    }
                    if (hdr.type == PKT_TYPE_ACK) {
                        ack_rcvd++;
                    }
                }
            }
            wait_ms += 50;
        }
        retries++;
    }

    if (!end_ms) {
        end_ms = now_ms();
    }

    double elapsed_ms = (start_ms && end_ms && end_ms > start_ms) ? (double)(end_ms - start_ms) : 1.0;
    double goodput_kbps = (file_size * 8.0) / (elapsed_ms);
    
    printf("FILE_BYTES=%llu\n", (unsigned long long)file_size);
    printf("CHUNK_BYTES=%d\n", MAX_PAYLOAD);
    printf("WIN=%d\n", win);
    printf("DATA_SENT_PKTS=%llu\n", (unsigned long long)data_sent);
    printf("DATA_RETX_PKTS=%llu\n", (unsigned long long)data_retx);
    printf("ACK_RCVD_PKTS=%llu\n", (unsigned long long)ack_rcvd);
    printf("ELAPSED_MS=%.0f\n", elapsed_ms);
    printf("GOODPUT_KBPS=%.2f\n", goodput_kbps);

    fclose(in);
    close(sock);
    return 0;
}
