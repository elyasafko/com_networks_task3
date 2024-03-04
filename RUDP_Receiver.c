#include <stdio.h> // Standard input/output library
#include <arpa/inet.h> // For the in_addr structure and the inet_pton function
#include <sys/socket.h> // For the socket function
#include <unistd.h> // For the close function
#include <string.h> // For the memset function
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "RUDP_API.h"



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

/*
 * @brief UDP Server main function.
 * @param None
 * @return 0 if the server runs successfully, 1 otherwise.
*/
int main(int argc, char *argv[]) 
{

    StrList *list = StrList_alloc();

    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in server;

    // The variable to store the client's address.
    struct sockaddr_in client;

    // Stores the client's structure length.
    socklen_t client_len = sizeof(client);

    // Create a message to send to the client.
    char *message = "Good morning, Vietnam\n";

    // Get the message length.
    int messageLen = strlen(message) + 1;

    // The variable to store the socket option for reusing the server's address.
    int opt = 1;

    // Reset the server and client structures to zeros.
    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    // Try to create a UDP socket (IPv4, datagram-based, default protocol).
    // sock = socket(AF_INET, SOCK_DGRAM, 0);
    sock = rudp_socket(true, atoi(argv[2]));

    if (sock == -1)
    {
        perror("socket(2)");
        return 1;
    }

    // Set the socket option to reuse the server's address.
    // This is useful to avoid the "Address already in use" error message when restarting the server.
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    // Set the server's address to "0.0.0.0" (all IP addresses on the local machine).
    server.sin_addr.s_addr = INADDR_ANY;

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the specified port. Note that the port must be in network byte order.
    server.sin_port = htons(atoi(argv[2]));

    // Try to bind the socket to the server's address and port.
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind(2)");
        close(sock);
        return 1;
    }

    fprintf(stdout, "Listening on port %d...\n", server.sin_port);

    // The server's main loop.
    while (1)
    {
        // Create a buffer to store the received message.
        char buffer[BUFFER_SIZE] = {0};

        fprintf(stdout, "Waiting for a message from a client...\n");

        // Receive a message from the client and store it in the buffer.
        int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client, &client_len);

        // If the message receiving failed, print an error message and return 1.
        // If the amount of received bytes is 0, the client sent an empty message, so we ignore it.
        if (bytes_received < 0)
        {
            perror("recvfrom(2)");
            close(sock);
            return 1;
        }

        // Ensure that the buffer is null-terminated, no matter what message was received.
        // This is important to avoid SEGFAULTs when printing the buffer.
        if (buffer[BUFFER_SIZE - 1] != '\0')
            buffer[BUFFER_SIZE- 1] = '\0';

        fprintf(stdout, "Received %d bytes from the client %s:%d: %s\n", bytes_received, inet_ntoa(client.sin_addr), ntohs(client.sin_port), buffer);

        // Send back a message to the client.
        int bytes_sent = sendto(sock, message, messageLen, 0, (struct sockaddr *)&client, client_len);

        // If the message sending failed, print an error message and return 1.
        // We do not need to check for 0 bytes sent, as if the client disconnected, we would have already closed the socket.
        if (bytes_sent < 0)
        {
            perror("sendto(2)");
            close(sock);
            return 1;
        }
        
        fprintf(stdout, "Sent %d bytes to the client %s:%d!\n", bytes_sent, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    }

    return 0;
}