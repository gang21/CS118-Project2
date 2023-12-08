#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"


int listen_for_ack(int sockfd, struct sockaddr_in addr, int beg_seq, int window_size) {
    socklen_t addr_size = sizeof(addr);
    char buffer[PAYLOAD_SIZE];
    struct packet pkt;
    int n;
    struct timeval  timeout;
    timeout.tv_sec = 0;    // wait 0 seconds
    timeout.tv_usec = 1000;   // wait 1000 milliseconds
    
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    if (beg_seq == -1) {     // stop and wait (window_size == 1)
        n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, &addr_size);
        if (n < 0) {
            perror("Error receiving ACK from server\n");
            // return -1;
        }
        if (n) {    // received ACK
            // printRecv(&pkt);
            printf("ACK received: %d", pkt.acknum);
            return pkt.acknum;
        }
    return pkt.acknum;
    }
    
    // aimd listen for acks for multiple packets
    for (int i = beg_seq; i < window_size + beg_seq; i++) {
        n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, &addr_size);    //FIXME: for some reason this only errors (returns -1)
        printf("expected ack: %d  | received ack: %d\n", i, pkt.acknum);
        printf("N: %d\n", n);
        if (n < 0) {
            perror("Error receiving ACK from server\n");
            // return -1;
        }
        if (pkt.acknum != i) {   // received ACKs out of order
            printf("out of order ack\n");
            // return -1;
        } 
        printf("ACK received: %d\n", pkt.acknum);
    }
    return beg_seq + window_size;
}

void send_file_data (FILE *fp, int sockfd, struct sockaddr_in addr, int listen_sockfd, struct sockaddr_in server_addr_from) {
    int n;
    char buffer[PAYLOAD_SIZE];
    int seq_no = 0;
    int ack_no = 0;

    while(fgets(buffer, PAYLOAD_SIZE, fp) != NULL) {
        struct packet pkt;
        build_packet(&pkt, seq_no, ack_no, 0, 0, PAYLOAD_SIZE, buffer);
        printf("SEND----------\n");
        printf("seq: %d\n", pkt.seqnum);
        printf("ack: %d\n", pkt.acknum);
        printf("data: %s", buffer);

        n = sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr));
        if (n == -1) {
            perror("Error sending data to the server");
            return;
        }

        //wait for ACK
        while(1) {
            int ack=-1;
            ack = listen_for_ack(listen_sockfd, server_addr_from, -1, -1);
            printf("RECEIVED ACK: %d\n", ack);
            if (ack != -1 && ack == ack_no) {    //ack received, move on to next packet
                break;
            }
            n = sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr));
                // if (n == -1) {
                //     perror("Error sending data to the server");
                //     return;
                // }
        }
        bzero(buffer, PAYLOAD_SIZE);
        printf("upping the ack\n");
        seq_no++;
        ack_no++;
        printf("seq: %d\n", seq_no);
        printf("ack: %d\n", ack_no);
    }
    // EOF
    struct packet last_packet;
    build_packet(&last_packet, seq_no, ack_no, 1, 0, PAYLOAD_SIZE, buffer);
    int ack = -1;
    sendto(sockfd, &last_packet, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
    while(1) {
        ack = listen_for_ack(listen_sockfd, server_addr_from, -1, -1);
        if (ack == -1) {    // error - resend data
            printSend(&last_packet, 1);
            n = sendto(sockfd, &last_packet, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
            if (n == -1) {
                perror("Error sending data to the server");
                return;
            }
            continue;
        }
    }

    fclose(fp);
    return;
}

int get_file_num_lines(FILE *fp) {
    int lines = 0;
    int ch;
    //count number lines
    while(!feof(fp)) {
        ch = fgetc(fp);
        if(ch == '\n') {
            lines++;
        }
    }
    //rewind file for reading
    fseek(fp, 0, SEEK_SET);
    return lines;
}

int send_multiple_packets(int sockfd, struct sockaddr_in addr, char (*buffer)[PAYLOAD_SIZE], int window_size, int seq_num, int file_size) {
    for (int i = 0; i < window_size; i++) {
        if (seq_num + i > file_size) {
            return 1;
        }
        // printf("data: %ssequence #: %d\n", buffer[i], seq_num+i);
        struct packet pkt;
        build_packet(&pkt, seq_num+i, seq_num+i, 0, 0, PAYLOAD_SIZE, buffer[seq_num + i]);
        int n = sendto(sockfd, &pkt, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (n == -1) {
            perror("Error sending data to the server");
            return -1;
        }
        // sleep(100);
    }
    return 0;

}

// bulk of the congestion control algorithm with aimd
void aimd(FILE *fp, int sockfd, struct sockaddr_in addr, int listen_sockfd, struct sockaddr_in server_addr_from) {
    double cwnd = 1;
    int window_size = (int)cwnd;
    int first_seq_of_window = 0;
    int file_size = get_file_num_lines(fp);
    printf("file size: %d\n", file_size);
    fseek(fp, 0, SEEK_SET);
    char buffer[file_size][PAYLOAD_SIZE];
    //reading entire file into buffer
    for (int j = 0; j <= file_size; j++) {
        fgets(buffer[j], sizeof(buffer), fp);
        printf("Line %d: %s", j, buffer[j]);
    }
    printf("\n------------------------------\n");
    while(1) {
    // for (int k = 0; k < 2; k++) {
        if (first_seq_of_window > file_size) {
            break;
        }
        int n = send_multiple_packets(sockfd, addr, buffer, window_size, first_seq_of_window, file_size);
        //listen for all acks
        /* if ack == -1, resend all the acks (redo for loop above)
           otherwise, if all the acks were received correctly, update the window_size and seq number*/
        while(1) {
            printf("first seq of window: %d\n", first_seq_of_window);
            int ack = listen_for_ack(listen_sockfd, server_addr_from, first_seq_of_window, window_size);
            printf("ACK: %d\n", ack);
            if (ack == -1) {
                cwnd = cwnd/2;
                window_size=(int)cwnd;
                send_multiple_packets(sockfd, addr, buffer, window_size, first_seq_of_window, file_size);
            }
            else {
                break;
            }
            // printf("------------------resending data--------------------\n");
            // cwnd = cwnd/2;
            // window_size=(int)cwnd;
            // send_multiple_packets(sockfd, addr, buffer, window_size, first_seq_of_window);
        }

        // acks were received in order, update cwnd and window size
        cwnd = cwnd + (1/cwnd);
        window_size = (int)cwnd;
        first_seq_of_window += (window_size-1);
        printf("first seq of window: %d\n", first_seq_of_window);
        printf("window size: %d\n", window_size);
        printf("---------------------------------------------\n");
    }


    

    // done sending - signal to server by sending packet with last flag == 1
    struct packet last_packet;
    build_packet(&last_packet, -1, -1, 1, 0, PAYLOAD_SIZE, buffer);
    int ack = -1;
    sendto(sockfd, &last_packet, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
    // while(1) {
    //     ack = listen_for_ack(listen_sockfd, server_addr_from, -1, -1);
    //     if (ack == -1) {    // error - resend data
    //         printSend(&last_packet, 1);
    //         int n = sendto(sockfd, &last_packet, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
    //         if (n == -1) {
    //             perror("Error sending data to the server");
    //             return;
    //         }
    //         continue;
    //     }
    // }
}

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    // struct packet pkt;
    // struct packet ack_pkt;
    // char buffer[PAYLOAD_SIZE];
    // unsigned short seq_num = 0;
    // unsigned short ack_num = 0;
    // char last = 0;
    // char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    // send_file_data(fp, send_sockfd, server_addr_to, listen_sockfd, server_addr_from);
    aimd(fp, send_sockfd, server_addr_to, listen_sockfd, server_addr_from);

    printf("[SUCCES] Sending file to server\n");
    printf("[CLOSING] Disconnecting from server\n");
    
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

