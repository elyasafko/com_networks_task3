#include <stdio.h> // Standard input/output library
#include <arpa/inet.h> // For the in_addr structure and the inet_pton function
#include <sys/socket.h> // For the socket function
#include <unistd.h> // For the close function
#include <string.h> // For the memset function
#include <sys/time.h> // For tv struct
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "RUDP_API.h"



#define BUFFER_SIZE 65536
#define MAX_WAIT_TIME 2


char *util_generate_random_data(unsigned int size) 
{
    char *buffer = NULL;
    
    // Argument check.
    if (size == 0)
        return NULL;
    
    buffer = (char *)calloc(size, sizeof(char));
    
    // Error checking.
    if (buffer == NULL)
        return NULL;
    
    // Randomize the seed of the random number generator.
    srand(time(NULL));
    
    for (unsigned int i = 0; i < size; i++)
        *(buffer + i) = ((unsigned int)rand() % 256);
    
    return buffer;
}



int main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE] = {0};

    // Generate some random data.
    unsigned int size = 2*1024*1024; //2MB
    char *message = util_generate_random_data(size);
    if (message == NULL)
    {
        perror("util_generate_random_data() failed");
        return -1;
    }

    // The variable to store the socket file descriptor.
    RUDP_Socket* sock = NULL;

    // The variable to store the server's address.
    struct sockaddr_in server;

    // Create a timeval struct to store the maximum wait time for the recvfrom(2) call.
    struct timeval tv = { MAX_WAIT_TIME, 0 };

    // Reset the server structure to zeros.
    memset(&server, 0, sizeof(server));

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    //sock = socket(AF_INET, SOCK_DGRAM, 0);
    sock = rudp_socket(false, 0);

    // If the socket creation failed, print an error message and return 1.
    if (sock == NULL)
    {
        perror("socket(2)");
        return 1;
    }
    /*
    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, argv[2], &server.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        rudp_close(sock);
        return 1;
    }
    */
    if (rudp_connect(sock, argv[2], atoi(argv[4])) == 0)
    {
        perror("rudp_connect() failed");
        rudp_close(sock);
        return 1;
    }

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the defined port. Note that the port must be in network byte order,
    // so we first convert it to network byte order using the htons function.
    server.sin_port = htons(atoi(argv[4]));

    // Since in UDP there is no connection, there isn't a guarantee that the server is up and running.
    // Therefore, we set a timeout for the recvfrom(2) call using the setsockopt function.
    // If the server does not respond within the timeout, the client will drop.
    // The timeout is set to 2 seconds by default.
    if (setsockopt(sock->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)) == -1)
    {
        perror("setsockopt(2)");
        rudp_close(sock);
        return 1;
    }

    fprintf(stdout, "Sending message to the server: %s\n", message);

    int total_bytes_sent = 0;
    int remaining_bytes = strlen(message);
    printf("message length: %d\n", remaining_bytes);

    while (remaining_bytes > 0) 
    {
        // Try to send the remaining part of the message to the server using the created socket and the server structure.
        int bytes_sent = rudp_send(sock, message + total_bytes_sent, BUFFER_SIZE);

        // If the message sending failed, print an error message and return 1.
        // If no data was sent, print an error message and return 1. In UDP, this should not happen unless the message is empty.
        if (bytes_sent <= 0) 
        {
            perror("sendto(2)");
            rudp_close(sock);
            return 1;
        }

        total_bytes_sent += bytes_sent;
        remaining_bytes -= bytes_sent;
    }

    fprintf(stdout, "Sent %d bytes to the server!\n"
                    "Waiting for the server to respond...\n", total_bytes_sent);

    // The variable to store the server's address, that responded to the message.
    // Note that this variable is not used in the client, but it is required by the recvfrom function.
    // Also note that the target server might be different from the server that responded to the message.
    struct sockaddr_in recv_server;

    // The variable to store the server's address length.
    socklen_t recv_server_len = sizeof(recv_server);

    // Try to receive a message from the server using the socket and store it in the buffer.
    // int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&recv_server, &recv_server_len);
    int bytes_received = rudp_recv(sock, buffer, BUFFER_SIZE);

    // If the message receiving failed, print an error message and return 1.
    // If no data was received, print an error message and return 1. In UDP, this should not happen unless the message is empty.
    if (bytes_received <= 0)
    {
        perror("recvfrom(2)");
        rudp_close(sock);
        return 1;
    }

    // Ensure that the buffer is null-terminated, no matter what message was received.
    // This is important to avoid SEGFAULTs when printing the buffer.
    if (buffer[BUFFER_SIZE - 1] != '\0')
        buffer[BUFFER_SIZE- 1] = '\0';

    // Print the received message, and the server's address and port.
    fprintf(stdout, "Got %d bytes from %s:%d: %s\n", bytes_received, inet_ntoa(recv_server.sin_addr), ntohs(recv_server.sin_port), buffer);

    // Close the socket UDP socket.
    rudp_close(sock);

    fprintf(stdout, "Client finished!\n");

    // Return 0 to indicate that the client ran successfully.
    return 0;
}