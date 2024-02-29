#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define BUFFER_SIZE 65536


/*
* @brief
A random data generator function based on srand() and rand().
* @param size
The size of the data to generate (up to 2^32 bytes).
* @return
A pointer to the buffer.
*/
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

    // Create a socket.
    int sock = -1;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket(2) failed");
        return -1;
    }

    // need to fix the TCP_CONGESTION
    if (strcmp(argv[6], "reno") == 0) 
    {
           printf("Setting TCP to Reno\n");
        if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, "reno", strlen("reno")) != 0) 
           {
               perror("setsockopt() failed");
               return -1;
           }
    } else if (strcmp(argv[6], "cubic") == 0) 
    {
           printf("Setting TCP to Cubic\n");
           if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, "cubic", strlen("cubic")) != 0) 
           {
               perror("setsockopt() failed");
               return -1;
           }
    } else {
           printf("Invalid TCP congestion control algorithm\n");
           return -1;
    }



    // Create a server address.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    // Set the server address.
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[4]));
    
    // Connect to the server.
    if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("connect() failed");
        close(sock);
        return -1;
    }
    printf("connected to server\n");

    char again;
    do 
    {
        // Send the data.
        int bytesSent = 0;
        while (bytesSent < size)
        {
            int ret = send(sock, message + bytesSent, size - bytesSent, 0);
            if (ret < 0)
            {
                perror("send(2)");
                close(sock);
                return 1;
            }
            bytesSent += ret;
            printf("Sent %d bytes\n", bytesSent);
        }
        char* finishMessage = "Finish\n";
        send(sock, finishMessage, strlen(finishMessage), 0);

        printf("Do you want to send the message again? (y/n): ");
        scanf(" %c", &again);
    } while (again == 'y');

    // Send exit message to the server
    printf("Sending exit message to the server\n");
    char* exitMessage = "Exit\n";
    send(sock, exitMessage, strlen(exitMessage), 0);
    printf("Exit message sent\n");

    // Receive a message from the server.
    int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0)
    {
        perror("recv(2)");
        close(sock);
        return 1;
    }

    // Ensure that the buffer is null-terminated, no matter what message was received.
    // This is important to avoid SEGFAULTs when printing the buffer.
    if (buffer[BUFFER_SIZE - 1] != '\0')
        buffer[BUFFER_SIZE- 1] = '\0';

    // Print the received message.
    //fprintf(stdout, "Got %d bytes from the server, which says: %s\n", bytes_received, buffer);

    // Close the socket with the server.
    close(sock);

    fprintf(stdout, "Connection closed!\n");

    free(message);
    // Return 0 to indicate that the client ran successfully.
    return 0;
}