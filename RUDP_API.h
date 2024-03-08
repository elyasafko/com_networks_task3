#ifndef RUDP_API_H
#define RUDP_API_H

#include <stdbool.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MSG_BUFFER_SIZE 65536
#define RUDP_SYN_FLAG 1
#define RUDP_ACK_FLAG 2
#define RUDP_FIN_FLAG 4
#define FILE_SIZE (1024 * 1024 * 2)
#define RETRY 3


typedef struct _rudp_socket {
  int socket_fd;
  bool isServer;
  bool isConnected;
  struct sockaddr_in dest_addr;
} RUDP_Socket;

RUDP_Socket *rudp_socket(bool isServer, unsigned short int listen_port);
int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port);
int rudp_accept(RUDP_Socket *sockfd);
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);
int rudp_disconnect(RUDP_Socket *sockfd);
int rudp_close(RUDP_Socket *sockfd);

#endif
