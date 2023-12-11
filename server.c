#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    // struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    struct packet ack_pkt;
    struct packet send_ack;
    unsigned short seq = 0;
    int n;

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
    while(1) {
        //receive from client
        n = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr_from, &addr_size);
        if (n < 0) {
            perror("Error sending ACK to the client");
            return -1;
        }

        //check if correct ack -- write to file
        if (ack_pkt.seqnum == seq) {
            // write to output.txt
            fwrite(ack_pkt.payload, 1, ack_pkt.length, fp);
            //send ack
            build_packet(&send_ack, ack_pkt.seqnum, ack_pkt.seqnum, 0, 1, 0, NULL);
            n = sendto(send_sockfd, &send_ack, sizeof(send_ack), 0, (struct sockaddr*)&client_addr_to, sizeof(client_addr_to));
            if (n < 0) {
                perror("Error sending ACK to client\n");
                break;
            }
            printf("the ack has been sent\n");
            //increment seq num
            seq++;

        }
        else {      // wrong packet
            build_packet(&send_ack, seq-1, seq-1, 0, 1, 0, NULL);
            n = sendto(send_sockfd, &send_ack, sizeof(send_ack), 0, (struct sockaddr*)&client_addr_to, sizeof(client_addr_to));
            if (n < 0) {
                perror("Error sending ACK to client\n");
                break;
            }
        }
        //check if last packet
        if (ack_pkt.last == 1) {
            printf("last! \n");
            break;
        }
    }

    printf("[SUCCES] File Transfer Complete\n");
    printf("[CLOSING] Closing server\n");

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}