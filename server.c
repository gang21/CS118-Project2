#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void write_file(int sockfd, struct sockaddr_in addr, FILE *fp) {
    int n;
    char buffer[PAYLOAD_SIZE];
    socklen_t addr_size;
    printf("WE R IN THIS FUNC\n");
    while(1){
        addr_size = sizeof(addr);
        printf("IN THE WHILE LOOP\n");
        n = recvfrom(sockfd, buffer, PAYLOAD_SIZE, 0, (struct sockaddr*)&addr, &addr_size);
        printf("N HAS BEEN RECEIVED: %d", n);
        if (strcmp(buffer, "END") == 0) {
            break; 
            return;
        }

        printf("[RECEIVING] Data: %s", buffer);
        fprintf(fp, "%s", buffer);
        bzero(buffer, PAYLOAD_SIZE);

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
    printf("WE GOT HERE\n");
    write_file(listen_sockfd, client_addr_from, fp);

    printf("[SUCCES] File Transfer Complete\n");
    printf("[CLOSING] Closing server\n");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
