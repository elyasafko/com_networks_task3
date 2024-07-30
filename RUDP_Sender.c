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

    printf("Starting RUDP Sender\n\n");

    // Generate some random data.
    unsigned int size = FILE_SIZE;
    char *message = util_generate_random_data(size);
    if (message == NULL)
    {
        perror("util_generate_random_data() failed");
        return 1;
    }
    printf("Generated %d bytes of random data\n", size);

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    int sock = udp_socket(argv[2], atoi(argv[4]));
    if (sock == -1)
    {
        perror("udp_socket() failed");
        return 1;
    }

    char again = 'y';
    while (again == 'y')
    {
        // Create RUDP socket
        if (rudp_socket(sock) < 0) {
            close(sock);
            return 1;
        }

        // Send the data.
        if (rudp_send(sock, message, size) <= 0)
        {
            printf("Could not send RUDP message\n");
            close(sock);
            return 1;
        }

        printf("Sent %d bytes to the server!\n\n", size);
        printf("Do you want to send the message again? (y/n): ");
        scanf(" %c", &again);
    }
    free(message);

    // Close the socket UDP socket.
    if (rudp_close(sock, 1) < 0) {
        close(sock);
        return 1;
    }

    printf("\nClient finished!\n");
    return 0;
}
