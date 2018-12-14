#ifndef CONNECTION_H
#define CONNECTION_H



#define RPC_MESSAGE WINSOCK_RPC_MESSAGE
#include <pthread.h>
#include <semaphore.h>

#include <winsock2.h>
#define MAX_CONNECTIONS 64
#define SOCKET_BUFFER_SIZE 8192
#undef RPC_MESSAGE

typedef struct CONNECTION_t
{
	SOCKET socket;
	SOCKADDR_IN addr;
	char buffer[SOCKET_BUFFER_SIZE];
	int size;
	int live;
	int port;
	pthread_t thread;
	sem_t mutex;
	sem_t empty;
} CONNECTION;


CONNECTION * ping(int src_port,int port); // Implementation in main.c
void connection_send(CONNECTION * connection, char * data, int size);
void connection_read(CONNECTION * connection, char * data, int size);


#endif