#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct packet pkt;
    struct packet ack_pkt;
    // struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    int last = 0;
    // char ack = 0;
    struct timeval  timeout;
    int n;

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
    while(last == 0) {
        //read from buffer
        long reset_pos = ftell(fp);
        size_t bytesRead = fread(buffer, 1, PAYLOAD_SIZE, fp);
        
        //check last packet
        if (bytesRead < PAYLOAD_SIZE) {
            last = 1;
        }
        else {
            last = 0;
        }
        //build packet
        build_packet(&pkt, seq_num, seq_num, last, 0, bytesRead, buffer);

        if (sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to)) < 0) {
            perror("Error sending to server\n");
            break;
        }

        // set timeout
        timeout.tv_sec = 0; 
        timeout.tv_usec = 100000;
        setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        n = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&server_addr_from, &addr_size);
        if (n < 0) {
            fseek(fp, reset_pos, SEEK_SET);
            continue;
        }

        if (ack_pkt.acknum == seq_num && ack_pkt.ack) {
            ack_num = 1;
        }
        else {
            ack_num = 0;
        }
        //expected ack is correct 
        if (ack_num) {
            seq_num++;
        }
        else {      // expected ack is incorrect
            fseek(fp, reset_pos, SEEK_SET);
            continue;
        }
    }

    printf("[SUCCES] Sending file to server\n");
    printf("[CLOSING] Disconnecting from server\n");
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

