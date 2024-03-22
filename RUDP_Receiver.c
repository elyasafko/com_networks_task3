#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "RUDP_API.h"
#include <time.h>

int main(int argc, char* argv[])
{
    if (argc != 3 || strcmp(argv[1], "-p") != 0)
    {
        printf("Invalid arguments\n");
        return 1;
    }

    int port = atoi(argv[2]);
    StrList* strList = StrList_alloc();

    printf("RUDP Receiver\n");

    // Create a UDP socket
    int sock = rudp_socket();
    if (sock == -1)
    {
        perror("Failed to create socket");
        return 1;
    }
    printf("Socket created\n");

    // Accept incoming connection requests
    if (rudp_accept(sock,port) == 0)
    {
        perror("Failed to accept connection");
        rudp_close(sock);
        return 1;
    }
    printf("Connection accepted\n");

    char buffer[MSG_BUFFER_SIZE];
    int round = 1;

    do
    {
        // Clear the buffer
        memset(buffer, 0, sizeof(buffer));

        // Capture start time
        clock_t start_time, end_time;
        
        // Receive data from the client in chunks
        int bytes_received;
        int firstround = 1;
        int totalBytes = 0;

        while ((bytes_received = rudp_recv(sock, buffer, sizeof(buffer), &strList)) > 0)
        {
            if (firstround)
            {
                start_time = clock();
                printf("Start receiving data\n");
            }

            if ((strstr(buffer, "Finish\n") != NULL) || (strstr(buffer, "Exit\n") != NULL))
            {
                printf("Received finish command. Exiting loop.\n");
                break;
            }

            // Handle received data
            if (bytes_received < 0)
            {
                perror("rudp_recv()");
                close(sock);
                StrList_free(strList);
                return 1;
            }
            else if (bytes_received == 0)
            {
                fprintf(stdout, "Client disconnected\n");
                close(sock);
                break;
            }
            else
            {
                // Ensure that the buffer is null-terminated
                if (buffer[MSG_BUFFER_SIZE - 1] != '\0')
                    buffer[MSG_BUFFER_SIZE - 1] = '\0';
            }

            firstround = 0;
            totalBytes += bytes_received;
            printf("Received %d bytes\n", bytes_received); // Add this line for debugging
            bzero(buffer, MSG_BUFFER_SIZE); // needed?
        }

        // Capture end time
        end_time = clock();
        printf("End receiving data\n");
        printf("Total bytes received: %d\n", totalBytes);

        // Calculate time difference in milliseconds
        double milliseconds = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

        StrList_insertLast(strList, round, milliseconds, totalBytes / milliseconds);
        fprintf(stdout, "Run #%d Data: Time: %fms Speed: %fMB/s\n", round, milliseconds, totalBytes / milliseconds);
        round++;
    } while (strstr(buffer, "Exit\n") == NULL);

    // Print statistics
    print_stats(strList);

    StrList_free(strList);

    rudp_close(sock);

    return 0;
}