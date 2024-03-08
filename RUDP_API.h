#ifndef RUDP_API_H
#define RUDP_API_H

#include <stdbool.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MSG_BUFFER_SIZE 65536
#define FILE_SIZE (1024 * 1024 * 2)
#define RETRY 3
#define TIMEOUT 1

typedef struct _RUDP_Flags 
{
    unsigned int SYN : 1;
    unsigned int ACK : 1;
    unsigned int DATA : 1;
    unsigned int FIN : 1;
} RUDP_flags;

typedef struct _RUDP_Packet 
{
    RUDP_flags flags;
    unsigned int length;
    unsigned int checksum;
    unsigned int seq_num;
    char data[MSG_BUFFER_SIZE];
} RUDP_Packet;

int rudp_socket();
int rudp_connect(int sock, const char *dest_ip, unsigned short int dest_port);
int rudp_accept(int sock);
int rudp_recv(int sock, void *buffer, unsigned int buffer_size);
int rudp_send(int sock, void *buffer, unsigned int buffer_size);
int rudp_disconnect(int sock);
int rudp_close(int sock);
int set_timeout(int socket, int time);
int receive_data_packet(int sock, void *buffer, RUDP_Packet *packet, int *sq_num);
int handle_fin_packet(int sock, RUDP_Packet *packet);
int receive_syn_packet(int sock, RUDP_Packet *packet, struct sockaddr_in *clientAddress, socklen_t *clientAddressLength);
int send_syn_ack_packet(int sock, const struct sockaddr_in *clientAddress, socklen_t clientAddressLength);
int send_syn_packet(int sock);
int receive_syn_ack_packet(int sock);
static int send_fin_and_wait_ack(int sock, RUDP_Packet *packet);
unsigned short int checksum(void *data, unsigned int bytes);
int send_ack(int socket, RUDP_Packet *packet);
int wait_for_ack(int socket, int seq_num, clock_t start_time, int timeout);


#endif
