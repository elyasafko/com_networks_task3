#ifndef RUDP_API_H
#define RUDP_API_H

#include <stdbool.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MSG_BUFFER_SIZE 16384
#define FILE_SIZE (1024 * 1024 * 2)
#define RETRY 5
#define TIMEOUT 5

typedef struct _RUDP_Flags 
{
    unsigned char SYN : 1;
    unsigned char ACK : 1;
    unsigned char DATA : 1;
    unsigned char FIN : 1;
} RUDP_flags;

typedef struct _RUDP_Packet 
{
    union {
        RUDP_flags flags;
        unsigned char all_flags;
    };
    unsigned short int length;
    unsigned short int checksum;
    unsigned short int seq_num;
    char data[MSG_BUFFER_SIZE];
} RUDP_Packet;

//******************* linked list ****************
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
} StrList, *pStrList;

/* Opens the socket. Sender: connect; Reciever: bind. */
int udp_socket(const char *dest_ip, unsigned short int dest_port);
/* Sender: sends SYN, waits for SYN+ACK */
int rudp_socket(int sock);
/* Reciever: connect + gets SYN+ACK or flags=0xFF for USP termination */
int rudp_accept(int sock, int port, int *done);
int rudp_recv(int sock, void *buffer, unsigned int buffer_size, pStrList *strList, int *done);
int rudp_send(int sock, void *buffer, unsigned int buffer_size);
int rudp_close(int sock, int send);
int receive_data_packet(int sock, void *buffer, RUDP_Packet *packet, int *sq_num);
int send_ack(int socket, RUDP_Packet *packet);
unsigned short int checksum(void *data, unsigned int bytes);

void print_stats(const StrList *strList);
void StrList_insertLast(StrList *strList, int run, double time, double speed);
size_t StrList_size(const StrList *strList);
void StrList_free(StrList *strList);
StrList *StrList_alloc();
void Node_free(Node *node);
Node *Node_alloc(int run, double time, double speed, Node *next);


#endif
