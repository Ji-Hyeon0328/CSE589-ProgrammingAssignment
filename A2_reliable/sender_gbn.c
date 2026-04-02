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
//#include <linux/time.h>

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

typedef struct{
    uint8_t bytes[PKT_HDR_LEN+MAX_PAYLOAD];
    size_t pktlen;
    uint32_t seq;

    int is_used;
} gbn_slot_t;

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
#pragma region exception
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
#pragma endregion

    uint8_t buf[PKT_HDR_LEN + MAX_PAYLOAD];
    uint8_t recvbuf[PKT_HDR_LEN + MAX_PAYLOAD];

    uint32_t base=0;
    uint32_t next_seq = 0;

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
    gbn_slot_t* window = calloc((size_t)win, sizeof(gbn_slot_t));
    if(!window){
        perror("window calloc");
        fclose(in);
        close(sock);
        return 1;
    }

    uint64_t timer_start_ms = 0;
    int timer_running =0;
    int eof_reached =0;

    while (1) { //!eof_reached || base <next_seq
        
        if(eof_reached && base >= next_seq){         
            break;
        }

        // waiting for window queing
        while (!eof_reached && next_seq < base + (uint32_t)win){
            size_t nread = fread(buf + PKT_HDR_LEN, 1, MAX_PAYLOAD, in);
            if (nread == 0){
                eof_reached=1;
                break;
            } 
            
            // Build a DATA packet: header + payload.
            size_t pktlen = pkt_build_data(buf, sizeof(buf), next_seq, buf + PKT_HDR_LEN, (uint16_t)nread);
            if (pktlen == 0) {
                fprintf(stderr, "packet build failed\n");
                free(window);
                fclose(in);
                close(sock);
                return 1;
            }

            gbn_slot_t* slot = &window[next_seq % win];
            memcpy(slot->bytes,buf,pktlen);
            slot->pktlen=pktlen;
            slot->seq=next_seq;
            slot->is_used=1;
            
            // Example data send call (goes through emulator).
            // TODO(student): in your reliable sender, send from a window buffer.
            if (netif_send(sock, buf, pktlen) < 0) {
                perror("sendto");
                free(window);
                fclose(in);
                close(sock);
                return 1;
            }            

            if (start_ms == 0) {
                start_ms = now_ms();
            }
            data_sent += 1;
            
            if(base==next_seq){
                timer_start_ms=now_ms();
                timer_running=1;
            }

            next_seq += 1;

        }
        
        // Example ACK receive path (non-blocking). We only count ACKs here.
        // TODO(student): use ACKs to slide the window and retransmit on timeout.
        ssize_t rn = netif_recv(sock, recvbuf, sizeof(recvbuf), 50);
        if (rn > 0) {
            pkt_hdr_t hdr;
            if (pkt_parse(recvbuf, (size_t)rn, &hdr, NULL, NULL) == 0) {
                if (hdr.type == PKT_TYPE_ACK) {
                    uint32_t ack = hdr.ack;

                    if(base<ack && ack <=next_seq){
                        ack_rcvd++;
                        uint32_t prev_base=base;
                        base=ack;

                        for (uint32_t s = prev_base;s<base;s++){
                            window[s%win].is_used=0;
                        }

                        if(base==next_seq){
                            timer_running=0;
                        }
                        else{
                            timer_start_ms=now_ms();
                            timer_running=1;
                        }
                    }
                printf("[SENDER] ACK=%u base=%u next_seq=%u\n", ack, base, next_seq);
                fflush(stdout);        
                }
            }
            
        }

        printf("[SENDER] TIMEOUT base=%u next_seq=%u\n", base, next_seq);
        fflush(stdout);
        if (timer_running && (now_ms()-timer_start_ms >= (uint64_t)rto_ms)){
            for (uint32_t s = base; s < next_seq; s++){
                gbn_slot_t* slot = &window[s % win];

                if(slot->is_used && slot->seq==s){
                    if(netif_send(sock,slot->bytes,slot->pktlen)<0){
                        perror("send Time out");
                        free(window);
                        fclose(in);
                        close(sock);
                        return 1;
                    }
                    data_retx=data_retx+1;
                }
            }
            timer_start_ms=now_ms();
        }

        
    }

    // Basic FIN send (no retransmission).
    // Build and send FIN to mark end of file.
    size_t fin_len = pkt_build_fin(buf, sizeof(buf), next_seq);
    if (fin_len > 0) {
        netif_send(sock, buf, fin_len);
    }

    // Basic wait for FINACK (no retries).
    uint64_t wait_ms = 0;
    while (wait_ms < (uint64_t)rto_ms) {
        ssize_t n = netif_recv(sock, recvbuf, sizeof(recvbuf), 50);
        if (n > 0) {
            pkt_hdr_t hdr;
            if (pkt_parse(recvbuf, (size_t)n, &hdr, NULL, NULL) == 0) {
                if (hdr.type == PKT_TYPE_FINACK) {
                    end_ms = now_ms();
                    break;
                }
                if (hdr.type == PKT_TYPE_ACK) {
                    ack_rcvd++;
                }
            }
        }
        wait_ms += 50;
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
