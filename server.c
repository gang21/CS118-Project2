#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void send_ack(int sockfd, struct sockaddr_in addr, unsigned short ack_num, unsigned short seq_num) {
    int n;
    char buffer[PAYLOAD_SIZE];
    memcpy(buffer, (char*)&ack_num, sizeof(unsigned int));
    struct packet pkt;
    build_packet(&pkt, seq_num, ack_num, 0, 1, PAYLOAD_SIZE, buffer);

    printf("Sending ACK: %d\n", ack_num);

    n = sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr));
    if (n == -1) {
        perror("Error sending ACK to the client");
        return;
    }
    bzero(buffer, PAYLOAD_SIZE);
    return;
}

void write_file(int listen_sockfd, struct sockaddr_in addr, FILE *fp, int send_sockfd, struct sockaddr_in client_addr_to) {
    int n;
    // char buffer[PAYLOAD_SIZE];
    socklen_t addr_size;
    int ack_num = 0;
    int seq_num = 0;
    struct packet pkt;

    while(1){
        addr_size = sizeof(addr);
        n = recvfrom(listen_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, &addr_size);
        printRecv(&pkt);
        if (pkt.last == 1) {
            break; 
        }
        if (pkt.acknum < ack_num) {   //dup ACK
            send_ack(send_sockfd, client_addr_to, ack_num, seq_num);
            continue;
        }
        fprintf(fp, "%s", pkt.payload);
        send_ack(send_sockfd, client_addr_to, ack_num, seq_num);
        // bzero(buffer, PAYLOAD_SIZE);

        //receive packet data, send ACK num

        ack_num += 1;

    }

    fclose(fp);
    return;
}

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    write_file(listen_sockfd, client_addr_from, fp, send_sockfd, client_addr_to);

    printf("[SUCCES] File Transfer Complete\n");
    printf("[CLOSING] Closing server\n");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
