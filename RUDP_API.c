#include "RUDP_API.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>

struct RUDPFlags
{
    unsigned int syn : 1;
    unsigned int ack : 1;
    unsigned int fin : 1;
};

struct RUDPPacket
{
    unsigned int length;
    unsigned int checksum;
    struct RUDPFlags flags;
    char msg[MSG_BUFFER_SIZE];
};

RUDP_Socket *rudp_socket(bool isServer, unsigned short int listen_port)
{
    RUDP_Socket *sockfd = (RUDP_Socket *)malloc(sizeof(RUDP_Socket));
    if (sockfd == NULL)
    {
        return NULL;
    }

    sockfd->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd->socket_fd == -1)
    {
        perror("socket(2) failed");
        free(sockfd);
        return NULL;
    }

    sockfd->isServer = isServer;
    sockfd->isConnected = false;

    if (isServer)
    {
        sockfd->dest_addr.sin_family = AF_INET;
        sockfd->dest_addr.sin_port = htons(listen_port);
        sockfd->dest_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sockfd->socket_fd, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr)) == -1)
        {
            perror("bind(2) failed");
            free(sockfd);
            return NULL;
        }
        printf("Listening on port %d...\n", listen_port);
    }

    return sockfd;
}

int rudp_handshake(RUDP_Socket *sockfd)
{
    struct RUDPPacket syn_packet;
    socklen_t addr_len = sizeof(sockfd->dest_addr);

    if (sockfd->isServer)
    {
        // Server waits for SYN packet from the client
        printf("Server waiting for SYN packet...\n");
        ssize_t recv_result = recvfrom(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);

        if (recv_result == -1)
        {
            perror("recvfrom(2) failed");
            return 0; // Handshake failed
        }

        if (syn_packet.flags.syn != 1)
        {
            printf("Handshake failed: SYN not received\n");
            return 0; // Handshake failed
        }

        // Send ACK packet to the client
        struct RUDPPacket ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet)); // Initialize the structure with zeros
        ack_packet.flags.ack = 1;

        ssize_t send_result = sendto(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
        if (send_result == -1)
        {
            perror("sendto(2) failed");
            return 0; // Handshake failed
        }
        printf("Server sent ACK packet...\n");
    }
    else
    {
        // Client sends SYN packet to the server
        memset(&syn_packet, 0, sizeof(syn_packet)); // Initialize the structure with zeros
        syn_packet.flags.syn = 1;

        ssize_t send_result = sendto(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
        if (send_result == -1)
        {
            perror("sendto(2) failed");
            return 0; // Handshake failed
        }
        printf("Client sent SYN packet...\n");

        // Receive ACK packet from the server
        ssize_t recv_result = recvfrom(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);

        if (recv_result == -1)
        {
            perror("recvfrom(2) failed");
            return 0; // Handshake failed
        }

        if (syn_packet.flags.ack != 1)
        {
            printf("Handshake failed: ACK not received\n");
            return 0; // Handshake failed
        }
        printf("Client received ACK packet...\n");
    }

    sockfd->isConnected = true;
    printf("Handshake successful!\n");
    return 1; // Handshake successful
}

int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port)
{
    if (sockfd->isServer || sockfd->isConnected)
    {
        return 0;
    }

    sockfd->dest_addr.sin_family = AF_INET;
    sockfd->dest_addr.sin_port = htons(dest_port);

    if (inet_pton(AF_INET, dest_ip, &(sockfd->dest_addr.sin_addr)) <= 0)
    {
        return 0;
    }

    if (connect(sockfd->socket_fd, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr)) == -1)
    {
        printf("connect failed\n");
        return 0;
    }

    // Perform handshake
    return rudp_handshake(sockfd);
}

int rudp_accept(RUDP_Socket *sockfd)
{
    if (!sockfd->isServer || sockfd->isConnected)
    {
        return 0;
    }
    sockfd->isConnected = true;
    return 1;
}

int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    if (!sockfd->isConnected)
    {
        return -1;
    }
    int bytes_received = recvfrom(sockfd->socket_fd, buffer, buffer_size, 0, NULL, NULL);
    if (bytes_received == 0)
    {
        sockfd->isConnected = false;
    }
    return bytes_received;
}

int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    if (!sockfd->isConnected)
    {
        return -1;
    }

    int bytes_sent = sendto(sockfd->socket_fd, buffer, buffer_size, 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    if (bytes_sent == -1)
    {
        perror("sendto(2) failed");
        sockfd->isConnected = false;
    }

    return bytes_sent;
}

int rudp_disconnect(RUDP_Socket *sockfd)
{
    if (!sockfd->isConnected)
    {
        return 0;
    }
    sockfd->isConnected = false;
    return 1;
}

int rudp_close(RUDP_Socket *sockfd)
{
    close(sockfd->socket_fd);
    free(sockfd);
    return 1;
}
