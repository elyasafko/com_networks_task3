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
        printf("Usage: %s -ip <server_ip> -p <port>\n", argv[0]);
        return 1;
    }

    char buffer[MSG_BUFFER_SIZE] = {0};

    // Generate some random data.
    unsigned int size = FILE_SIZE;
    char *message = util_generate_random_data(size);
    if (message == NULL)
    {
        perror("util_generate_random_data() failed");
        return -1;
    }
    printf("Generated %d bytes of random data\n", size);

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    int sock = rudp_socket();
    if (sock == -1)
    {
        perror("rudp_socket() failed");
        return 1;
    }
    printf("Created an RUDP socket\n");

    if (rudp_connect(sock, argv[2], atoi(argv[4])) <= 0)
    {
        perror("rudp_connect() failed");
        rudp_close(sock);
        free(message);
        return 1;
    }
    printf("Connected to server at %s:%s\n", argv[2], argv[4]);

    char again = 'y';
    while (again == 'y')
    {
        // Send the data.
        int total_bytes_sent = 0;
        int remaining_bytes = size;
        if (rudp_send(sock, message, size) <= 0)
        {
            perror("rudp_send() failed");
            rudp_close(sock);
            free(message);
            return 1;
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
    printf("Closed the RUDP socket\n");
    free(message);
    return 0;
}
