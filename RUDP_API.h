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
#define RETRY 3
#define TIMEOUT 5

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
int rudp_accept(int sock, int port);
int rudp_recv(int sock, void *buffer, unsigned int buffer_size, pStrList *strList);
int rudp_send(int sock, void *buffer, unsigned int buffer_size);
int rudp_close(int sock);
int receive_data_packet(int sock, void *buffer, RUDP_Packet *packet, int *sq_num);
int wait_for_ack(int socket, int seq_num, clock_t start_time, int timeout);
int handle_fin_packet(int sock, RUDP_Packet *packet);
int send_ack(int socket, RUDP_Packet *packet);
unsigned short int checksum(void *data, unsigned int bytes);

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

void print_stats(const StrList *strList);
void StrList_insertLast(StrList *strList, int run, double time, double speed);
size_t StrList_size(const StrList *strList);
void StrList_free(StrList *strList);
StrList *StrList_alloc();
void Node_free(Node *node);
Node *Node_alloc(int run, double time, double speed, Node *next);


#endif
