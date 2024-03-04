// RUDP_API.h

#ifndef RUDP_API_H
#define RUDP_API_H

#include <stdbool.h>
#include <netinet/in.h>

typedef struct _rudp_socket 
{
    int socket_fd;
    bool isServer;
    bool isConnected;
    struct sockaddr_in dest_addr;
} RUDP_Socket;

RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port);
int rudp_connect(RUDP_Socket* sockfd, const char* dest_ip, unsigned short int dest_port);
int rudp_accept(RUDP_Socket* sockfd);
int rudp_recv(RUDP_Socket* sockfd, void* buffer, unsigned int buffer_size);
int rudp_send(RUDP_Socket* sockfd, void* buffer, unsigned int buffer_size);
int rudp_disconnect(RUDP_Socket* sockfd);
int rudp_close(RUDP_Socket* sockfd);

#endif
