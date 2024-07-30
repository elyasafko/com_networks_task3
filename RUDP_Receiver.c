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

    printf("Starting RUDP Receiver\n\n");

    // Create a UDP socket
    int sock = udp_socket(NULL, port);
    if (sock == -1)
    {
        return 1;
    }


    char buffer[MSG_BUFFER_SIZE];
    int round = 1;
    int done, bytes_received, totalBytes;
    StrList* strList = StrList_alloc();

    do
    {
        done = 0;

        // Accept incoming connection requests
        if (rudp_accept(sock, port, &done) < 0)
        {
            perror("Failed to accept connection");
            rudp_close(sock, 0);
            return 1;
        }

        // Clear the buffer
        memset(buffer, 0, sizeof(buffer));

        // Capture start time
        clock_t start_time, end_time;
        
        // Receive data from the client in chunks
        int firstround = 1;

        while ((done == 0) && ((bytes_received = rudp_recv(sock, buffer, sizeof(buffer), &strList, &done)) >= 0))
        {
            if (firstround)
            {
                firstround = 0;
                totalBytes = 0;
                start_time = clock();
            }
            totalBytes += bytes_received;
            printf("Got %d bytes of data.  Total %d bytes\n", bytes_received, totalBytes);
        }

        if (done > 0) {
            // Capture end time
            end_time = clock();
            printf("End receiving data\n");

            // Calculate time difference in milliseconds
            double milliseconds = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
            StrList_insertLast(strList, round, milliseconds, totalBytes / milliseconds);
            printf("Run #%d Data: Time: %fms Speed: %fMB/s\n\n", round, milliseconds, totalBytes / milliseconds);
            round++;
        }
    } while (done > 0);

    rudp_close(sock, 0);

    // Print statistics
    print_stats(strList);

    StrList_free(strList);

    printf("\nReceiver finished!\n");

    return 0;
}
