#include "RUDP_API.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Define flags for the RUDP header
#define RUDP_SYN_FLAG 0x01
#define RUDP_ACK_FLAG 0x02

// RUDP header structure
struct RUDPHeader 
{
    uint16_t length;
    uint16_t checksum;
    uint8_t flags;
};

RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port) 
{
    RUDP_Socket* sockfd = (RUDP_Socket*)malloc(sizeof(RUDP_Socket));
    if (sockfd == NULL) 
    {
        return NULL;
    }

    sockfd->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd->socket_fd == -1) 
    {
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

        if (bind(sockfd->socket_fd, (struct sockaddr*)&(sockfd->dest_addr), sizeof(sockfd->dest_addr)) == -1) 
        {
            free(sockfd);
            return NULL;
        }
    }
    
    return sockfd;
}

int rudp_handshake(RUDP_Socket* sockfd) 
{
    struct RUDPHeader syn_packet;
    socklen_t addr_len = sizeof(sockfd->dest_addr);

    if (sockfd->isServer) 
    {
        // Server waits for SYN packet from the client
        recvfrom(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr*)&(sockfd->dest_addr),
                 &addr_len);

        if (syn_packet.flags != RUDP_SYN_FLAG) 
        {
            return 0; // Handshake failed
        }

        // Send ACK packet to the client
        struct RUDPHeader ack_packet;
        ack_packet.length = 0;
        ack_packet.checksum = 0;
        ack_packet.flags = RUDP_ACK_FLAG;

        sendto(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&(sockfd->dest_addr),
               sizeof(sockfd->dest_addr));

    } else {
        // Client sends SYN packet to the server
        syn_packet.length = 0;
        syn_packet.checksum = 0;
        syn_packet.flags = RUDP_SYN_FLAG;

        sendto(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr*)&(sockfd->dest_addr),
               sizeof(sockfd->dest_addr));

        // Receive ACK packet from the server
        recvfrom(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr*)&(sockfd->dest_addr),
                 &addr_len);

        if (syn_packet.flags != RUDP_ACK_FLAG) 
        {
            return 0; // Handshake failed
        }
    }

    sockfd->isConnected = true;
    return 1; // Handshake successful
}

int rudp_connect(RUDP_Socket* sockfd, const char* dest_ip, unsigned short int dest_port) 
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

    // Perform handshake
    return rudp_handshake(sockfd);
}

// Accepts incoming connection request and completes the handshake, returns 0 on failure and 1 on success. Fails if called when the socket is connected/set to client.
int rudp_accept(RUDP_Socket *sockfd)
{
    if (!sockfd->isServer || sockfd->isConnected)
    {
        return 0;
    }
    sockfd->isConnected = true;
    return 1;
}

// Receives data from the other side and put it into the buffer. Returns the number of received bytes on success, 0 if got FIN packet (disconnect), and -1 on error. Fails if called when the socket is disconnected.
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

// Sends data stores in buffer to the other side. Returns the number of sent bytes on success, 0 if got FIN packet (disconnect), and -1 on error. Fails if called when the socket is disconnected.
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    if (!sockfd->isConnected)
    {
        return -1;
    }
    int bytes_sent = sendto(sockfd->socket_fd, buffer, buffer_size, 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
    if (bytes_sent == 0)
    {
        sockfd->isConnected = false;
    }
    return bytes_sent;

}

// Disconnects from an actively connected socket. Returns 1 on success, 0 when the socket is already disconnected (failure).
int rudp_disconnect(RUDP_Socket *sockfd)
{
    if (!sockfd->isConnected)
    {
        return 0;
    }
    sockfd->isConnected = false;
    return 1;
}

// This function releases all the memory allocation and resources of the socket.
int rudp_close(RUDP_Socket *sockfd)
{
    close(sockfd->socket_fd);
    free(sockfd);
    return 1;
}