#include "RUDP_API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

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

    // setup a timeout for the socket
    if (set_timeout(sock, TIMEOUT) == -1)
    {
        perror("set_timeout failed");
        return -1;
    }

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
    if (send_syn_packet(sock) == -1)
        return -1;

    // receive SYN-ACK message
    if (receive_syn_ack_packet(sock) == -1)
        return -1;

    printf("Connected\n");
    return 1;
}

int send_syn_packet(int sock)
{
    RUDP_Packet packet;
    memset(&packet, 0, sizeof(RUDP_Packet)); // zero out the packet
    packet.flags.SYN = 1; // set the SYN flag

    int total_tries = 0; // total number of tries

    while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
    {
        int send_result = sendto(sock, &packet, sizeof(RUDP_Packet), 0, NULL, 0);
        if (send_result == -1)
        {
            perror("sendto failed");
            return -1;
        }

        total_tries++;
    }

    printf("Could not send SYN packet\n");
    return 0;
}

int receive_syn_ack_packet(int sock)
{
    int total_tries = 0; // total number of tries

    while (total_tries < RETRY) // while the total number of tries is less than the maximum number of tries
    {
        RUDP_Packet recv_packet;
        memset(&recv_packet, 0, sizeof(RUDP_Packet));

        int recv_result = recvfrom(sock, &recv_packet, sizeof(RUDP_Packet), 0, NULL, 0);
        if (recv_result == -1)
        {
            perror("recvfrom failed");
            return -1;
        }

        if (recv_packet.flags.SYN && recv_packet.flags.ACK)
        {
            return 1;
        }
        else
        {
            printf("Received wrong packet when trying to connect\n");
        }

        total_tries++;
    }

    printf("Could not receive SYN-ACK packet\n");
    return 0;
}


int rudp_accept(int sock)
{
    // Add print statements for better debugging
    printf("Waiting for connection...\n");

    // setup a timeout for the socket
    if (set_timeout(sock, TIMEOUT) == -1)
    {
        printf("set_timeout failed");
        return -1;
    }

    // Setup the client address structure.
    struct sockaddr_in clientAddress; // client address
    memset(&clientAddress, 0, sizeof(clientAddress)); // zero out the structure
    socklen_t clientAddressLength = sizeof(clientAddress); // client address length

    // receive SYN message
    RUDP_Packet packet;
    memset(&packet, 0, sizeof(RUDP_Packet)); // zero out the packet

    int recv_result = receive_syn_packet(sock, &packet, &clientAddress, &clientAddressLength);
    if (recv_result == -1) // if the receive failed
    {
        perror("receive_syn_packet failed");
        return -1;
    }

    if (packet.flags.SYN) // if the received packet is a SYN packet
    {
        printf("Received SYN\n");

        // send SYN-ACK message
        int send_result = send_syn_ack_packet(sock, &clientAddress, clientAddressLength);
        if (send_result == -1) // if the send failed
        {
            perror("send_syn_ack_packet failed");
            return -1;
        }

        printf("Sent SYN-ACK\n");
        return 1;
    }
    else
    {
        printf("Received wrong packet when trying to accept\n");
    }

    return 0;
}

int receive_syn_packet(int sock, RUDP_Packet *packet, struct sockaddr_in *clientAddress, socklen_t *clientAddressLength)
{
    int recv_result = recvfrom(sock, packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)clientAddress, clientAddressLength); // receive the packet
    free(packet); // free the allocated memory
    printf("Received SYN\n");
    return recv_result;
}

int send_syn_ack_packet(int sock, const struct sockaddr_in *clientAddress, socklen_t clientAddressLength)
{
    RUDP_Packet ack_packet;
    memset(&ack_packet, 0, sizeof(RUDP_Packet)); // zero out the packet
    ack_packet.flags.SYN = 1; // set the SYN flag
    ack_packet.flags.ACK = 1; // set the ACK flag

    int send_result = sendto(sock, &ack_packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)clientAddress, clientAddressLength); // send the packet

    return send_result;
}


int rudp_recv(int sock, void *buffer, unsigned int buffer_size)
{
    int sq_num = 0;
    RUDP_Packet packet;
    memset(&packet, 0, sizeof(RUDP_Packet));

    int recv_result = recvfrom(sock, &packet, sizeof(RUDP_Packet), 0, NULL, 0);
    if (recv_result == -1)
    {
        perror("recvfrom failed");
        return -1;
    }

    // Check if the packet is corrupted
    if (checksum(packet.data, sizeof(packet.data)) != packet.checksum)
    {
        return -1;
    }

    // Send ACK for the received packet
    if (send_ack(sock, &packet) == 0)
    {
        return -1;
    }

    // Check if the packet is a SYN packet
    if (packet.flags.SYN == 1)
    {
        printf("Received SYN\n");
        return 0;
    }

    if (packet.seq_num == sq_num)
    {
        if (packet.seq_num == 0 && packet.flags.DATA == 1)
            set_timeout(sock, TIMEOUT * 10);

        int result = receive_data_packet(sock, buffer, &packet, &sq_num);
        return result;
    }
    else if (packet.flags.DATA == 1)
    {
        return 0;
    }

    if (packet.flags.FIN == 1)
    {
        printf("Received close request\n");
        int result = handle_fin_packet(sock, &packet);
        return result;
    }

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
    if (recv_packet == NULL) {
        perror("malloc failed");
        return -1;
    }

    for (int i = 0; i < packet_amount; i++) // for each packet to send
    {
        RUDP_Packet *packet = malloc(sizeof(RUDP_Packet)); // allocate memory for the packet
        if (packet == NULL) {
            perror("malloc failed");
            free(recv_packet);
            return -1;
        }

        memset(packet, 0, sizeof(RUDP_Packet)); // zero out the packet
        packet->flags.DATA = 1; // set the DATA flag
        packet->seq_num = id_packet; // set the sequence number

        // Set the FIN flag for the last packet
        if (i == packet_amount - 1 && buffer_size % MSG_BUFFER_SIZE == 0)
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
                    perror("recvfrom failed");
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

int rudp_disconnect(int sock)
{
    RUDP_Packet disconnect_pk;
    memset(&disconnect_pk, 0, sizeof(RUDP_Packet));
    disconnect_pk.flags.FIN = 1;

    strncpy(disconnect_pk.data, "Disconnect", sizeof(disconnect_pk.data) - 1);

    disconnect_pk.checksum = checksum(disconnect_pk.data, strlen(disconnect_pk.data) + 1);

   while (wait_for_ack(sock, 0, clock(), TIMEOUT) == -1) 
   {
        if (send_fin_and_wait_ack(sock, &disconnect_pk) == -1) 
        {
            return -1;
        }
    }

    return 0;
}

int rudp_close(int sock)
{
    RUDP_Packet close_pk;
    memset(&close_pk, 0, sizeof(RUDP_Packet));
    close_pk.flags.FIN = 1;
    close_pk.seq_num = -1;
    close_pk.checksum = checksum(close_pk.data, sizeof(close_pk.data));

    do
    {
        int sendResult = sendto(sock, &close_pk, sizeof(RUDP_Packet), 0, NULL, 0);
        if (sendResult == -1)
        {
            perror("sendto failed");
            return -1;
        }
    } while (wait_for_ack(sock, -1, clock(), TIMEOUT) == -1);

    if (close(sock) == -1)
    {
        perror("close failed");
        return -1;
    }

    return 0;  // or return 1; depending on your convention
}

int set_timeout(int sock, int time_sec)
{
    if (time_sec <= 0) 
    {
        fprintf(stderr, "Error: Invalid timeout value\n");
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = time_sec;
    timeout.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) 
    {
        printf("Error setting timeout for socket");
        return -1;
    }
    printf("Timeout set to %d seconds\n", time_sec);
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

int send_ack(int socket, RUDP_Packet *packet) 
{
  RUDP_Packet ack_packet;
  memset(&ack_packet, 0, sizeof(RUDP_Packet));

  ack_packet.flags.ACK = 1;
  ack_packet.flags.FIN = packet->flags.FIN;
  ack_packet.flags.SYN = packet->flags.SYN;
  ack_packet.flags.DATA = packet->flags.DATA;

  ack_packet.seq_num = packet->seq_num;

  // Update the checksum calculation based on your actual data
  ack_packet.checksum = checksum(ack_packet.data, sizeof(ack_packet.data));

  int sendResult = sendto(socket, &ack_packet, sizeof(RUDP_Packet), 0, NULL, 0);
  
  if (sendResult == -1) 
  {
    printf("sendto failed\n");
    return 0;
  }

  return 1;
}

static int send_fin_and_wait_ack(int sock, RUDP_Packet *packet) 
{
    int sendResult = sendto(sock, packet, sizeof(RUDP_Packet), 0, NULL, 0);
    if (sendResult == -1) 
    {
        perror("sendto failed");
        return -1;
    }

    return wait_for_ack(sock, packet->seq_num, clock(), TIMEOUT);
}