#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"


int listen_for_ack(int sockfd, struct sockaddr_in addr) {
    socklen_t addr_size = sizeof(addr);
    char buffer[PAYLOAD_SIZE];
    struct packet pkt;
    int n;
    struct timeval  timeout;
    timeout.tv_sec = 0;    // wait 0 seconds
    timeout.tv_usec = 1000;   // wait 4000 milliseconds
    
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, &addr_size);
    if (n < 0) {
        perror("Error receiving ACK from server\n");
        return -1;
    }
    if (n) {    // received ACK
        // printRecv(&pkt);
        return pkt.acknum;
    }
    return pkt.acknum;
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
            ack = listen_for_ack(listen_sockfd, server_addr_from);
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

    struct packet last_packet;
    build_packet(&last_packet, seq_no, ack_no, 1, 0, PAYLOAD_SIZE, buffer);
    int ack = -1;
    sendto(sockfd, &last_packet, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
    while(1) {
        ack = listen_for_ack(listen_sockfd, server_addr_from);
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
    send_file_data(fp, send_sockfd, server_addr_to, listen_sockfd, server_addr_from);

    printf("[SUCCES] Sending file to server\n");
    printf("[CLOSING] Disconnecting from server\n");
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

