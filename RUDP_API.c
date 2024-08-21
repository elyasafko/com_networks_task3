#include "RUDP_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stddef.h>

int seq_num; // id of the expected packet

void rudp_dump_data(RUDP_Packet *packet)
{
    for (int q=0; q<offsetof(RUDP_Packet, data)+32; q+=16)
        printf("0x%04X: %08X %08X %08X %08X\n", q,
                *(int *)((char *)packet + q),
                *(int *)((char *)packet + q + 4),
                *(int *)((char *)packet + q + 8),
                *(int *)((char *)packet + q + 12));
}

#define rudp_dump_headers(x, p) \
    printf("%03d: " x " SYN %d ACK %d DATA %d FIN %d length %05d checksum %04X seq_num %d\n", __LINE__, p->flags.SYN, p->flags.ACK, p->flags.DATA, p->flags.FIN, p->length, p->checksum, p->seq_num)

int udp_socket(const char *dest_ip, unsigned short int dest_port) 
{
    if (dest_ip == NULL) {
        printf("Setting up UDP at port %u...\n", dest_port);
    }
    else
    {
        printf("Setting up UDP at %s:%u...\n", dest_ip, dest_port);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) 
    {
        perror("Error creating UDP socket");
        return -1;
    }

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
    if (dest_ip)
    {
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
    }
    else
    {
        // bind the socket to the server address
        if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) 
        {
            perror("bind() failed");
            close(sock);
            return -1;
        }
    }

    printf("UDP socket created for %s:%u...\n\n", inet_ntoa(serverAddress.sin_addr), dest_port);
    return sock;
}

int rudp_socket(int sock)
{
    // send SYN message
    RUDP_Packet *packet = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
    if (packet == NULL)
    {
        perror("malloc failed");
        return -1;
    }
    memset(packet, 0, sizeof(RUDP_Packet)); // zero out the packet
    packet->flags.SYN = 1; // set the SYN flag
    packet->seq_num = seq_num = 0;
    packet->checksum = checksum(packet->data, packet->length);

    int total_tries = 0; // total number of tries

    while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
    {
        rudp_dump_headers("OUT", packet);
        int send_result = sendto(sock, packet, sizeof(RUDP_Packet), 0, NULL, 0);
        if (send_result == -1)
        {
            perror("sendto() failed");
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
            printf("%d: Waiting for RUDP socket [seq_num %d]\n", __LINE__, seq_num);
            int recv_result = recvfrom(sock, recv_packet, sizeof(RUDP_Packet), 0, NULL, NULL);
            if (recv_result == -1)
            {
                perror("recvfrom() failed");
                free(recv_packet);
                free(packet);
                return -1;
            }
            rudp_dump_headers("IN ", recv_packet);

            if (recv_packet->flags.SYN && recv_packet->flags.ACK)
            {
                free(recv_packet);
                free(packet);
                printf("RUDP connected\n");
                return 0;
            }
            printf("Received wrong packet when trying to connect\n");

            inner_total_tries++;
            free(recv_packet);
        }
        printf("Could not receive SYN-ACK packet\n");
        total_tries++;
    }

    printf("Could not establish RUDP socket after %d retries\n", RETRY);
    free(packet); // free allocated memory

    return -1;
}

int rudp_accept(int sock, int port, int *done)
{
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

    printf("%d: Waiting for RUDP socket\n", __LINE__);
    int recv_result = recvfrom(sock, packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (recv_result == -1) 
    {
        perror("recvfrom() failed");
        free(packet);
        return -1;
    }
    rudp_dump_headers("IN ", packet);

    if (packet->all_flags == 0xFF)
    {
        *done = -1;
        free(packet);
        return 0;
    }

    if (connect(sock, (struct sockaddr *)&clientAddress, clientAddressLength) == -1) 
    {
        perror("connect() failed");
        free(packet);
        return -1;
    }

    if (packet->flags.SYN == 1) // if the received packet is a SYN packet
    {
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
        syn_ack_packet->seq_num = seq_num = packet->seq_num; // Initialize sequence number
        syn_ack_packet->checksum = checksum(syn_ack_packet->data, syn_ack_packet->length);

        rudp_dump_headers("OUT", syn_ack_packet);
        int send_result = sendto(sock, syn_ack_packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&clientAddress, clientAddressLength); // send the packet
        if (send_result == -1) // if the send failed
        {
            perror("sendto() failed");
            free(packet);
            free(syn_ack_packet);
            return -1;
        }

        free(packet);
        free(syn_ack_packet);

        seq_num++;
        printf("RUDP connected\n");
        return 0;
    }

    printf("Received wrong packet when trying to accept\n");
    free(packet);

    return -1;
}


int rudp_recv(int sock, void *buffer, unsigned int buffer_size, pStrList *strList, int *done)
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

    int total_tries = 0; // total number of tries
    while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
    {
        printf("%d: Waiting for RUDP socket [seq_num %d]\n", __LINE__, seq_num);
        int recv_result = recvfrom(sock, packet, sizeof(RUDP_Packet), 0, NULL, NULL);
        if (recv_result != -1)
            break;

        perror("recvfrom() failed");
        total_tries++; // increment the total number of tries
    }

    if (total_tries == RETRY) // if the total number of tries is equal to the maximum number of tries
    {
        printf("Could not recv packet %d\n", seq_num); // print an error message;
        free(packet); // free the packet
        return -1; // return an error
    }

    rudp_dump_headers("IN ", packet);

    // Check if the packet is corrupted
    if (checksum(packet->data, packet->length) != packet->checksum)
    {
        printf("checksum error: 0x%08X 08%08X\n", checksum(packet->data, packet->length), packet->checksum);
        free(packet);
        return 0;
    }

    // Check if the packet is a SYN packet
    if (packet->flags.SYN)
    {
        seq_num = 0; // Initialize sequence number
        free(packet);
        return 0;
    }

    if ((packet->seq_num > seq_num) || (packet->seq_num - seq_num < -1))
    {
        printf("seq_num error: packet %d expected %d\n", packet->seq_num, seq_num);
        free(packet);
        return -1;
    }

    // Send ACK for the received packet
    if (send_ack(sock, packet) < 0)
    {
        free(packet);
        return -1;
    }

    if (packet->flags.FIN)
    {
        *done = 1;
    }

    if (packet->flags.DATA)
    {
        int len = packet->length;
        memcpy(buffer, packet->data, len);

        if (packet->seq_num == seq_num)
            seq_num++;
        else
            printf("seq_num mismatch, not incrementing it: packet %d expected %d\n", packet->seq_num, seq_num);

        free(packet);
        return len;
    }

    free(packet);
    return 0;
}

int rudp_send(int sock, void *buffer, unsigned int buffer_size)
{
    // number of packets to send.  Last data packet must be partial, even if buffer_size==MSG_BUFFER_SIZE.
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
        packet->seq_num = ++seq_num; // set the sequence number

        // Set the FIN flag for the last packet
        if (i == packet_amount - 1)
        {
            packet->flags.FIN = 1;
            // set the length of the packet
            packet->length = buffer_size;
        }
        else
        {
            // set the length of the packet
            packet->length = MSG_BUFFER_SIZE;
            buffer_size -= packet->length;
        }

        memcpy(packet->data, buffer + i * MSG_BUFFER_SIZE, packet->length); // copy the data to the packet

        // Calculate the checksum for the packet
        packet->checksum = checksum(packet->data, packet->length);

        int total_tries = 0; // total number of tries
        while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
        {
            rudp_dump_headers("OUT", packet);
            ssize_t send_result = sendto(sock, packet, offsetof(RUDP_Packet, data) + packet->length, 0, NULL, 0); // send the packet
            if (send_result == -1) // if the send failed
            {
                perror("sendto() failed");
                free(packet);
                free(recv_packet);
                return -1;
            }

            clock_t start_time = clock(); // start the timer

            do
            {
                // receive ACK message
                printf("%d: Waiting for RUDP socket [seq_num %d]\n", __LINE__, seq_num);
                ssize_t recv_result = recvfrom(sock, recv_packet, sizeof(RUDP_Packet), 0, NULL, 0); // receive the packet
                if (recv_result == -1) // if the receive failed
                {
                    perror("recvfrom() failed");
                    break;
                }
                rudp_dump_headers("IN ", recv_packet);

                if (recv_packet->flags.ACK && recv_packet->seq_num == seq_num) // if the received packet is an ACK packet
                {
                    if (recv_packet->flags.FIN)
                    {
                        printf("RUDP disconnected\n");
                    }
                    total_tries = RETRY;   // no need to retry
                    break; // break the loop
                }
            } while ((double)(clock() - start_time) / CLOCKS_PER_SEC < TIMEOUT); // while the time elapsed is less than the timeout

            total_tries++; // increment the total number of tries
        }

        if (total_tries == RETRY) // if the total number of tries is equal to the maximum number of tries
        {
            printf("Could not send packet %d\n", seq_num); // print an error message;
            free(packet); // free the packet
            free(recv_packet);
            return -1; // return an error
        }

        free(packet); // free the packet
    }

    free(recv_packet); // free the received packet
    return 1; // return success
}

int rudp_close(int sock, int send)
{
    if (send)
    {
        RUDP_Packet *close_pk = (RUDP_Packet *)malloc(sizeof(RUDP_Packet));
        if (close_pk == NULL)
        {
            perror("malloc failed");
            return -1;
        }
        memset(close_pk, 0, sizeof(RUDP_Packet));
        close_pk->all_flags = 0xFF;   // special case to signal RUDP connection ended

        rudp_dump_headers("OUT", close_pk);
        int sendResult = sendto(sock, close_pk, sizeof(RUDP_Packet), 0, NULL, 0);
        if (sendResult == -1)
        {
            perror("sendto() failed");
            free(close_pk);
            return -1;
        }
        free(close_pk);
    }

    close(sock);

    printf("UDP socket closed\n");
    return 0;
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
        return -1;
    }
    memset(ack_packet, 0, sizeof(RUDP_Packet));

    ack_packet->flags.ACK = 1;
    ack_packet->flags.FIN = packet->flags.FIN;
    ack_packet->flags.SYN = packet->flags.SYN;
    ack_packet->seq_num = packet->seq_num;
    ack_packet->checksum = checksum(ack_packet->data, ack_packet->length);

    rudp_dump_headers("OUT", ack_packet);
    if (sendto(socket, ack_packet, sizeof(RUDP_Packet), 0, NULL, 0) == -1)
    {
        perror("sendto() failed");
        free(ack_packet);
        return -1;
    }
    if (ack_packet->flags.FIN)
    {
        printf("RUDP disconnected\n");
    }
    free(ack_packet);
    return 0;
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
    if (strList->_size < 1)
        return;

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
