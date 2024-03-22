#include "RUDP_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>


int rudp_socket() 
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) 
    {
        perror("Error creating RUDP socket");
        return -1;
    }
    return sock;
}

int rudp_connect(int sock, const char *dest_ip, unsigned short int dest_port)
{
    // Add print statements for better debugging
    printf("Connecting to %s:%u...\n", dest_ip, dest_port);

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) 
    {
        printf("Error setting timeout for socket");
        return -1;
    }
    printf("Timeout set to %d seconds\n", TIMEOUT);

    // Setup the server address structure.
    struct sockaddr_in serverAddress; // server address
    memset(&serverAddress, 0, sizeof(serverAddress)); // zero out the structure
    serverAddress.sin_family = AF_INET; // IPv4 address family
    serverAddress.sin_port = htons(dest_port); // server port
    int rval = inet_pton(AF_INET, dest_ip, &serverAddress.sin_addr); // convert IP address to network address
    if (rval <= 0) // if the conversion failed
    {
        perror("inet_pton() failed");
        return -1;
    }

    // connect to the server
    if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("connect failed");
        return -1;
    }

    // send SYN message
    RUDP_Packet *packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
    if (packet == NULL)
    {
        perror("malloc failed");
        return -1;
    }
    memset(packet, 0, sizeof(RUDP_Packet)); // zero out the packet
    packet->flags.SYN = 1; // set the SYN flag

    int total_tries = 0; // total number of tries

    while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
    {
        printf("Sending SYN packet..."); // print the number of tries
        int send_result = sendto(sock, packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
        if (send_result == -1)
        {
            perror("sendto failed");
            free(packet); // free allocated memory
            return -1;
        }

        // Attempt to receive SYN-ACK packet immediately after sending SYN
        int inner_total_tries = 0; // total number of tries
        while (inner_total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
        {
            RUDP_Packet *recv_packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet)); // allocate memory for the received packet
            if (recv_packet == NULL)
            {
                perror("malloc failed");
                return -1;
            }
            memset(recv_packet, 0, sizeof(RUDP_Packet));
            socklen_t serverAddrLen = sizeof(serverAddress);
            int recv_result = recvfrom(sock, recv_packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&serverAddress, &serverAddrLen);
            if (recv_result == -1)
            {
                perror("recvfrom failed");
                free(recv_packet);
                free(packet);
                return -1;
            }

            if (recv_packet->flags.SYN && recv_packet->flags.ACK)
            {
                free(recv_packet);
                free(packet);
                printf("Connected\n");
                return 1;
            }
            else
            {
                printf("Received wrong packet when trying to connect\n");
            }

            inner_total_tries ++;
            if (inner_total_tries  == RETRY)
            {
                free(recv_packet);
                free(packet);
            }
        }
        printf("Could not receive SYN-ACK packet\n");
        total_tries++;
    }

    printf("Could not send SYN packet after %d retries\n", RETRY);
    free(packet); // free allocated memory
    return -1;
}

int rudp_accept(int sock, int port)
{
    printf("Waiting for connection...\n");

    // Setup the server address structure.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the socket to the server address
    if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) 
    {
        perror("bind() failed");
        close(sock);
        return -1;
    }

    // Setup the client address structure.
    struct sockaddr_in clientAddress; // client address
    memset(&clientAddress, 0, sizeof(clientAddress)); // zero out the structure
    socklen_t clientAddressLength = sizeof(clientAddress); // client address length

    // receive SYN message
    RUDP_Packet *packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
    if (packet == NULL) 
    {
        perror("malloc failed");
        return -1;
    }
    memset(packet, 0, sizeof(RUDP_Packet)); // zero out the packet

    int recv_result = recvfrom(sock, packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (recv_result == -1) 
    {
        perror("recvfrom() failed");
        free(packet);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&clientAddress, clientAddressLength) == -1) 
    {
        perror("connect() failed");
        free(packet);
        return -1;
    }

    if (packet->flags.SYN == 1) // if the received packet is a SYN packet
    {
        printf("Received SYN\n");
        // send SYN-ACK message
        RUDP_Packet *syn_ack_packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet)); // allocate memory for the packet
        if (syn_ack_packet == NULL) 
        {
            perror("malloc failed");
            free(packet);
            return -1;
        }
        memset(syn_ack_packet, 0, sizeof(RUDP_Packet)); // zero out the packet
        syn_ack_packet->flags.SYN = 1; // set the SYN flag
        syn_ack_packet->flags.ACK = 1; // set the ACK flag

        int send_result = sendto(sock, syn_ack_packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&clientAddress, clientAddressLength); // send the packet
        if (send_result == -1) // if the send failed
        {
            perror("sendto() failed");
            free(packet);
            free(syn_ack_packet);
            return -1;
        }

        printf("Sent SYN-ACK\n");
        free(packet);
        free(syn_ack_packet);
        return 1;
    }
    else
    {
        printf("Received wrong packet when trying to accept\n");
        free(packet);
    }

    return 0;
}

int rudp_recv(int sock, void *buffer, unsigned int buffer_size, pStrList *strList)
{
    RUDP_Packet *packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
    if (packet == NULL)
    {
        perror("malloc failed");
        return -1;
    }
    memset(packet, 0, sizeof(RUDP_Packet));

    // Receive packet with error handling and timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT; // Set timeout in seconds
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed");
        free(packet);
        return -1;
    }

    int recv_result = recvfrom(sock, packet, sizeof(RUDP_Packet), 0, NULL, NULL);
    if (recv_result == -1)
    {
        perror("recvfrom failed");
        free(packet);
        return -1;
    }

    // Check if the packet is corrupted
    if (checksum(packet->data, sizeof(packet->data)) != packet->checksum)
    {
        free(packet);
        return -1;
    }

    // Send ACK for the received packet
    if (send_ack(sock, packet) == 0)
    {
        free(packet);
        return -1;
    }

    // Check if the packet is a SYN packet
    if (packet->flags.SYN == 1)
    {
        free(packet);
        printf("Received SYN\n");
        return 0;
    }

    int sq_num = 0; // Initialize sequence number

    if (packet->seq_num == sq_num)
    {
        int result = receive_data_packet(sock, buffer, packet, &sq_num);
        free(packet);
        return result;
    }
    else if (packet->flags.DATA == 1)
    {
        free(packet);
        return 0;
    }

    if (packet->flags.FIN == 1)
    {
        printf("Received close request\n");
        int result = handle_fin_packet(sock, packet);
        free(packet);
        return result;
    }

    free(packet);
    return 0;
}

int receive_data_packet(int sock, void *buffer, RUDP_Packet *packet, int *sq_num)
{
    if (packet->flags.FIN == 1 && packet->flags.DATA == 1)
    { // last packet
        memcpy(buffer, packet->data, packet->length);
        (*sq_num)++;
        return 1;
    }

    if (packet->flags.DATA == 1)
    { // data packet
        memcpy(buffer, packet->data, packet->length);
        (*sq_num)++;
        return 1;
    }

    return 0;
}

int handle_fin_packet(int sock, RUDP_Packet *packet)
{
    // close request
    // send ACK and wait for TIMEOUT*10 seconds to check if the sender closed
    printf("Received close request\n");
    set_timeout(sock, TIMEOUT * 10);

    time_t FIN_send_time = time(NULL);
    printf("Waiting for close\n");

    while ((double)(time(NULL) - FIN_send_time) < TIMEOUT)
    {
        memset(packet, 0, sizeof(RUDP_Packet));
        int recv_result = recvfrom(sock, packet, sizeof(RUDP_Packet) - 1, 0, NULL, 0);
        if (recv_result == -1)
        {
            perror("recvfrom failed");
            return -1;
        }

        if (packet->flags.FIN == 1)
        {
            if (send_ack(sock, packet) == -1)
            {
                return -1;
            }
            FIN_send_time = time(NULL);
        }
    }

    close(sock);
    return -2;
}

int rudp_send(int sock, void *buffer, unsigned int buffer_size)
{
    int id_packet = 0; // id of the packet
    int packet_amount = buffer_size / MSG_BUFFER_SIZE + (buffer_size % MSG_BUFFER_SIZE != 0); // number of packets to send

    RUDP_Packet *recv_packet = malloc(sizeof(RUDP_Packet)); // allocate memory for the received packet
    if (recv_packet == NULL) 
    {
        perror("malloc failed");
        return -1;
    }

    for (int i = 0; i < packet_amount; i++) // for each packet to send
    {
        RUDP_Packet *packet = malloc(sizeof(RUDP_Packet)); // allocate memory for the packet
        if (packet == NULL) 
        {
            perror("malloc failed");
            free(recv_packet);
            return -1;
        }

        memset(packet, 0, sizeof(RUDP_Packet)); // zero out the packet
        packet->flags.DATA = 1; // set the DATA flag
        packet->seq_num = id_packet; // set the sequence number

        // Set the FIN flag for the last packet
        if (i == packet_amount - 1)
        {
            packet->flags.FIN = 1;
        }

        packet->length = (i == packet_amount - 1) ? buffer_size % MSG_BUFFER_SIZE : MSG_BUFFER_SIZE; // set the length of the packet

        memcpy(packet->data, buffer + i * MSG_BUFFER_SIZE, packet->length); // copy the data to the packet

        // Calculate the checksum for the packet
        packet->checksum = checksum(packet->data, sizeof(packet->data));

        int total_tries = 0; // total number of tries
        while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
        {
            ssize_t send_result = sendto(sock, packet, sizeof(RUDP_Packet), 0, NULL, 0); // send the packet
            if (send_result == -1) // if the send failed
            {
                perror("sendto failed");
                free(packet);
                free(recv_packet);
                return -1;
            }

            clock_t start_time = clock(); // start the timer

            do
            {
                // receive ACK message
                ssize_t recv_result = recvfrom(sock, recv_packet, sizeof(RUDP_Packet), 0, NULL, 0); // receive the packet
                if (recv_result == -1) // if the receive failed
                {
                    printf("recvfrom failed");
                    free(packet);
                    free(recv_packet);
                    return -1;
                }

                if (recv_packet->flags.ACK && recv_packet->seq_num == id_packet) // if the received packet is an ACK packet
                {
                    break; // break the loop
                }
            } while ((double)(clock() - start_time) / CLOCKS_PER_SEC < TIMEOUT); // while the time elapsed is less than the timeout

            total_tries++; // increment the total number of tries
        }

        if (total_tries == RETRY) // if the total number of tries is equal to the maximum number of tries
        {
            printf("Could not send packet %d\n", id_packet); // print an error message
            free(packet); // free the packet
            return -1; // return an error
        }

        id_packet++; // increment the id of the packet
        free(packet); // free the packet
    }

    free(recv_packet); // free the received packet
    return 1; // return success
}

int rudp_close(int sock)
{
    RUDP_Packet *close_pk = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
    if (close_pk == NULL)
    {
        perror("malloc failed");
        return -1;
    }
    memset(close_pk, 0, sizeof(RUDP_Packet));
    close_pk->flags.FIN = 1;
    close_pk->seq_num = -1;
    close_pk->checksum = checksum(close_pk->data, sizeof(close_pk->data));

    do
    {
        int sendResult = sendto(sock, close_pk, sizeof(RUDP_Packet), 0, NULL, 0);
        if (sendResult == -1)
        {
            perror("sendto failed");
            free(close_pk);
            return -1;
        }
    } while (wait_for_ack(sock, -1, clock(), TIMEOUT) == -1);

    if (close(sock) == -1)
    {
        free(close_pk);
        perror("close failed");
        return -1;
    }
    free(close_pk);
    return 0;  // or return 1; depending on your convention
}


unsigned short int checksum(void *data, unsigned int bytes) 
{
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1) 
    {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
    total_sum += *((unsigned char *)data_pointer);
    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);

    return (~((unsigned short int)total_sum));
}

int send_ack(int socket, RUDP_Packet *packet) 
{
    RUDP_Packet *ack_packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
    if (ack_packet == NULL) 
    {
        perror("malloc failed");
        return 0;
    }
    memset(ack_packet, 0, sizeof(RUDP_Packet));

    ack_packet->flags.ACK = 1;
    ack_packet->flags.FIN = packet->flags.FIN;
    ack_packet->flags.SYN = packet->flags.SYN;
    ack_packet->flags.DATA = packet->flags.DATA;

    ack_packet->seq_num = packet->seq_num;

    // Update the checksum calculation based on your actual data
    ack_packet->checksum = checksum(ack_packet->data, sizeof(ack_packet->data));

    int sendResult = sendto(socket, &ack_packet, sizeof(RUDP_Packet), 0, NULL, 0);

    if (sendResult == -1) 
    {
        printf("sendto failed\n");
        free(ack_packet);
        return 0;
    }
    free(ack_packet);
    return 1;
}

int wait_for_ack(int socket, int seq_num, clock_t start_time, int timeout) 
{
    RUDP_Packet *packetReply = malloc(sizeof(RUDP_Packet));

    // Calculate the expiration time (start_time + timeout seconds)
    clock_t expiration_time = start_time + timeout * CLOCKS_PER_SEC;

    while (clock() < expiration_time) 
    {
        int recvLen = recvfrom(socket, packetReply, sizeof(RUDP_Packet) - 1, 0, NULL, 0);
        if (recvLen == -1) 
        {
            free(packetReply);
            return -1;
        }

        if (packetReply->seq_num == seq_num && packetReply->flags.ACK == 1) 
        {
            free(packetReply);
            return 1;
        }
    }
    free(packetReply);
    return -1;
}




// ************ Linked List **************

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