#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 65536
#define MAX_PENDING_CONNECTIONS 1

typedef struct _node 
{
    int _run;
    double _time;
    double _speed;
    struct _node * _next;
}Node;

typedef struct _StrList
{
    Node *_head;
    size_t _size;
}StrList;

Node* Node_alloc(int run, double time, double speed, Node* next)
{
	Node* p = (Node*)malloc(sizeof(Node));
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

void Node_free(Node* node) 
{
	free(node);
}

/*
 * Allocates a new empty StrList.
 * It's the user responsibility to free it with StrList_free.
 */
StrList* StrList_alloc()
{
    StrList* p = (StrList*)malloc(sizeof(StrList));
	p->_head= NULL;
	p->_size= 0;
	return p;
}


/*
 * Frees the memory and resources allocated to StrList.
 * If StrList==NULL does nothing (same as free).
 */
void StrList_free(StrList* StrList)
{
    if (StrList==NULL) return;
	Node* p1= StrList->_head;
	Node* p2;
	while(p1) 
	{
		p2= p1;
		p1= p1->_next;
		Node_free(p2);
	}
	free(StrList);
}

/*
 * Returns the number of elements in the StrList.
 */
size_t StrList_size(const StrList* StrList)
{
    return StrList->_size;
}

/*
 * Inserts an element in the end of the StrList.
 */
void StrList_insertLast(StrList* StrList,int run, double time, double speed)
{
    Node* p = Node_alloc(run, time, speed, NULL);
    if(StrList->_head == NULL) 
    {
        StrList->_head = p;
    }
    else 
    {
        Node* q = StrList->_head;
        while(q->_next) 
        {
            q = q->_next;
        }
        q->_next = p;
    }
    StrList->_size++;
}

void cleanup(int listeningSocket, int clientSocket) 
{
    close(clientSocket);
    close(listeningSocket);
}

int main(int argc, char *argv[]) 
{
    char *message = "Exit\n";
    StrList *list = StrList_alloc();

    // Create a socket.
    int listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1) 
    {
        perror("Could not create listening socket");
        return -1;
    }

    // Set the SO_REUSEADDR option.
    int enableReuse = 1;
    if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int)) == -1) 
    {
        perror("setsockopt(2)");
        cleanup(listeningSocket, -1);
        return 1;
    }

    // Bind the socket to the server address.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    // Fill the server address structure.
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[2]));
    serverAddress.sin_addr.s_addr = INADDR_ANY;


    // something  missing here? ip to something - inet pton


    // Bind the server address to the socket.
    if (bind(listeningSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) 
    {
        perror("bind() failed");
        cleanup(listeningSocket, -1);
        return -1;
    }
    printf("Bind success\n");

    // Listen for incoming connections.
    if (listen(listeningSocket, MAX_PENDING_CONNECTIONS) == -1) 
    {
        perror("listen() failed");
        cleanup(listeningSocket, -1);
        return -1;
    }

    printf("Waiting for incoming TCP-connections...\n");

    // Accept an incoming connection.
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);

    while (1) 
    {
        // Accept the connection.
        int clientSocket = accept(listeningSocket, (struct sockaddr *)&clientAddress, &clientAddressLen);

        // Check for errors.
        if (clientSocket == -1) 
        {
            perror("accept() failed");
            cleanup(listeningSocket, -1);
            return -1;
        }
        printf("Connection accepted\n");

        // Receive data from the client.
        char buffer[BUFFER_SIZE] = {0};

        // create a list to store the data
        int round = 1;

        do
        {
            // Clear the buffer
            memset(buffer, 0, sizeof(buffer));

            // Capture start time
            clock_t start_time, end_time;
            //gettimeofday(&start_time, NULL);

            // Receive data from client in chunks
            int bytes_received;
            int firstround = 1;
            int totalBytes = 0;
            while ((bytes_received = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) 
            {
                if (firstround)
                {
                    start_time = clock();
                    printf("start receiving data\n");
                }
                if ((strstr(buffer, "Finish\n") != NULL) || (strstr(buffer, "Exit\n") != NULL))
                {
                    printf("Received finish command. Exiting loop.\n");
                    break;
                }

                // Handle received data
                if (bytes_received < 0) 
                {
                    perror("recv(2)");
                    cleanup(listeningSocket, clientSocket);
                    return 1;
                } else if (bytes_received == 0) 
                {
                    fprintf(stdout, "Client %s:%d disconnected\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
                    cleanup(listeningSocket, clientSocket);
                    break;
                } else 
                {
                    // Ensure that the buffer is null-terminated
                    if (buffer[BUFFER_SIZE - 1] != '\0')
                        buffer[BUFFER_SIZE - 1] = '\0';
                }

                firstround = 0;
                totalBytes += bytes_received;
                bzero(buffer, BUFFER_SIZE); // needed?
            }
            // Capture end time
            end_time = clock();
            printf("end receiving data\n");
            printf("Total bytes received: %d\n", totalBytes);

            // Calculate time difference in milliseconds
            double milliseconds = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;



            StrList_insertLast(list, round, milliseconds, bytes_received / milliseconds);
            fprintf(stdout, "Run #%d Data: Time: %fms Speed: %fMB/s\n", round, milliseconds, bytes_received / milliseconds);
            round++;
        } while (strstr(buffer, "Exit\n") == NULL);


        // Send back a message to the client.
        int bytes_sent = send(clientSocket, message, strlen(message), 0);
        printf("Sending exit message to the client\n");

        // Handle sent data
        if (bytes_sent < 0) 
        {
            perror("send(2)");
            cleanup(listeningSocket, clientSocket);
            return 1;
        }

        // Close the client's socket
        cleanup(listeningSocket, clientSocket);
        fprintf(stdout, "Client %s:%d disconnected\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
        break;
    }

    fprintf(stdout, "Server finished!\n\n");

    printf("-----------------------------\n");
    printf("Stats:\n");
    printf("CC Algorithm: %s\n", argv[4]);
    printf("Number of runs: %zu\n", list->_size-1);
    Node* current = list->_head; // Use a temporary pointer to traverse the list

    for (int i = 0; i < list->_size-1; i++)
    {
        printf("Run #%d Data: Time: %f ms, Speed: %f MB/s\n", current->_run, current->_time, current->_speed);
        current = current->_next;
    }

    // Reset temporary pointer to head for calculating averages
    current = list->_head;

    float totalTime = 0.0;
    float totalSpeed = 0.0;

    for (int i = 0; i < list->_size-1; i++)
    {
        totalTime += current->_time;
        totalSpeed += current->_speed;
        current = current->_next;
    }

    printf("Average Time: %f ms\n", totalTime / list->_size);
    printf("Average Speed: %f MB/s\n", totalSpeed / list->_size);
    printf("-----------------------------\n");

    StrList_free(list);
    return 0;

}
