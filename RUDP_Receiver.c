#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "RUDP_API.h"
#include <time.h>

#define BUFFER_SIZE MSG_BUFFER_SIZE

typedef struct _Node
{
    int _run;
    double _time;
    double _speed;
    struct _Node *_next;
} Node;

typedef struct _StrList
{
    Node *_head;
    size_t _size;
} StrList;

Node *Node_alloc(int run, double time, double speed, Node *next)
{
    Node *p = (Node *)malloc(sizeof(Node));
    if (p == NULL)
    {
        return NULL;
    }
    p->_run = run;
    p->_time = time;
    p->_speed = speed;
    p->_next = next;
    return p;
}

void Node_free(Node *node)
{
    free(node);
}

StrList *StrList_alloc()
{
    StrList *p = (StrList *)malloc(sizeof(StrList));
    p->_head = NULL;
    p->_size = 0;
    return p;
}

void StrList_free(StrList *strList)
{
    if (strList == NULL)
        return;
    Node *p1 = strList->_head;
    Node *p2;
    while (p1)
    {
        p2 = p1;
        p1 = p1->_next;
        Node_free(p2);
    }
    free(strList);
}

size_t StrList_size(const StrList *strList)
{
    return strList->_size;
}

void StrList_insertLast(StrList *strList, int run, double time, double speed)
{
    Node *p = Node_alloc(run, time, speed, NULL);
    if (strList->_head == NULL)
    {
        strList->_head = p;
    }
    else
    {
        Node *q = strList->_head;
        while (q->_next)
        {
            q = q->_next;
        }
        q->_next = p;
    }
    strList->_size++;
}

void print_stats(const StrList *strList)
{
    printf("-----------------------------\n");
    printf("Stats:\n");
    printf("Number of runs: %zu\n", strList->_size);
    Node *current = strList->_head;

    for (size_t i = 0; i < strList->_size; i++)
    {
        printf("Run #%d Data: Time: %f ms, Speed: %f MB/s\n", current->_run, current->_time, current->_speed);
        current = current->_next;
    }

    current = strList->_head;

    double totalTime = 0.0;
    double totalSpeed = 0.0;

    for (size_t i = 0; i < strList->_size; i++)
    {
        totalTime += current->_time;
        totalSpeed += current->_speed;
        current = current->_next;
    }

    printf("Average Time: %f ms\n", totalTime / strList->_size);
    printf("Average Speed: %f MB/s\n", totalSpeed / strList->_size);
    printf("-----------------------------\n");
}

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

    char buffer[BUFFER_SIZE];
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

        while ((bytes_received = rudp_recv(sock, buffer, sizeof(buffer))) > 0)
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
                if (buffer[BUFFER_SIZE - 1] != '\0')
                    buffer[BUFFER_SIZE - 1] = '\0';
            }

            firstround = 0;
            totalBytes += bytes_received;
            printf("Received %d bytes\n", bytes_received); // Add this line for debugging
            bzero(buffer, BUFFER_SIZE); // needed?
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