#include <stdbool.h> // Include the header file for the bool type.
#include <netinet/in.h> // Include the header file for the sockaddr_in structure.
#include <stdlib.h> // Include the header file for the malloc function.
#include <string.h> // Include the header file for the memset function.


// A struct that represents RUDP Socket
typedef struct _rudp_socket
{
int socket_fd; // UDP socket file descriptor

bool isServer; // True if the RUDP socket acts like a server, false for client.
bool isConnected; // True if there is an active connection, false otherwise.

struct sockaddr_in dest_addr; // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;

// Allocates a new structure for the RUDP socket (contains basic information about the socket itself). Also creates a UDP socket as a baseline for the RUDP. isServer means that this socket acts like a server. If set to server socket, it also binds the socket to a specific port.
RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port)
{
    RUDP_Socket *sockfd = (RUDP_Socket *)malloc(sizeof(RUDP_Socket));
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
        if (bind(sockfd->socket_fd, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr)) == -1)
        {
            free(sockfd);
            return NULL;
        }
    }
    return sockfd;
}
// Tries to connect to the other side via RUDP to given IP and port. Returns 0 on failure and 1 on success. Fails if called when the socket is connected/set to server.
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
    sockfd->isConnected = true;
    return 1;
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