#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "RUDP_API.h"

#define BUFFER_SIZE MSG_BUFFER_SIZE
#define MAX_WAIT_TIME 2
#define EXIT_MSG "exit"
#define FILE_SIZE (1024 * 1024 * 2)

char *util_generate_random_data(unsigned int size)
{
    char *buffer = NULL;

    if (size == 0)
        return NULL;

    buffer = (char *)calloc(size, sizeof(char));

    if (buffer == NULL)
        return NULL;

    srand(time(NULL));

    for (unsigned int i = 0; i < size; i++)
        *(buffer + i) = ((unsigned int)rand() % 256);

    return buffer;
}

int main(int argc, char *argv[])
{
    if (argc != 5 || strcmp(argv[1], "-ip") != 0 || strcmp(argv[3], "-p") != 0)
    {
        printf("Invalid arguments\n");
        return 1;
    }

    char buffer[BUFFER_SIZE] = {0};

    // Generate some random data.
    unsigned int size = FILE_SIZE;
    char *message = util_generate_random_data(size);
    if (message == NULL)
    {
        perror("util_generate_random_data() failed");
        return -1;
    }
    printf("Generated %d bytes of random data\n", size);

    // The variable to store the socket file descriptor.
    RUDP_Socket *sock = NULL;

    // The variable to store the server's address.
    struct sockaddr_in server;

    // Create a timeval struct to store the maximum wait time for the recvfrom(2) call.
    struct timeval tv = {MAX_WAIT_TIME, 0};

    // Reset the server structure to zeros.
    memset(&server, 0, sizeof(server));

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    sock = rudp_socket(false, 0);
    if (sock == NULL)
    {
        perror("rudp_socket() failed");
        return 1;
    }
    printf("Created a RUDP socket\n");
    
    if (rudp_connect(sock, argv[2], atoi(argv[4])) == 0)
    {
        perror("rudp_connect() failed");
        rudp_close(sock);
        return 1;
    }
    fprintf(stdout, "Connected to server at %s:%s\n", argv[2], argv[4]);

    char again = 'y';
    while (again == 'y')
    {
        // Send the data.
        int total_bytes_sent = 0;
        int remaining_bytes = size;
    
        while (remaining_bytes > 0)
        {
            int bytes_sent = rudp_send(sock, message + total_bytes_sent, BUFFER_SIZE);
            if (bytes_sent == -1)
            {
                perror("rudp_send() failed");
                rudp_close(sock);
                free(message);
                return 1;
            }
            total_bytes_sent += bytes_sent;
            remaining_bytes -= bytes_sent;
            printf("Sent %d bytes\n", bytes_sent); // Add this line for debugging
        }
    
        printf("Sent %d bytes to the server!\n", total_bytes_sent);
        printf("Do you want to send the message again? (y/n): ");
        scanf(" %c", &again);
    
        // Reset total_bytes_sent and remaining_bytes for the next iteration
        total_bytes_sent = 0;
        remaining_bytes = size;
    }

    // The loop completed successfully, and all bytes were sent.
    fprintf(stdout, "Client finished!\n");

    // Close the socket UDP socket.
    rudp_close(sock);
    free(message);

    // Return 0 to indicate that the client ran successfully.
    return 0;
}
