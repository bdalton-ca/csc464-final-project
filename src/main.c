#include <stdlib.h>
#include <stdio.h>

#include <conio.h>

#include "dht.h"
#include "connection.h"
	
pthread_t server_thread;

CONNECTION connections[MAX_CONNECTIONS];
WSADATA WSAData;

void * client_socket_thread(void* data)
{
	CONNECTION * connection = (CONNECTION*)data;
 	
	sem_post( &connection->mutex );
	for(;;)
	{
		sem_wait( &connection->mutex );

		if(connection->buffer[0]=='\0')
		{
			sem_post( &connection->mutex );
			
			int n = 0; char tmp[SOCKET_BUFFER_SIZE];

			n = recv(connection->socket, tmp, sizeof(tmp), 0);
			
			if(n<=0) break;
			
			//printf("received: %s\n",tmp);
			sem_wait( &connection->mutex );
			connection->size = n;
			if(connection->live==0)
			{
				sem_post( &connection->mutex );
				break;
			}
			memcpy(connection->buffer,tmp,sizeof(tmp));
			
		}
		
		sem_post( &connection->mutex );
	}
	
	
	sem_wait( &connection->mutex );
	printf("CLOSING CONNECTION\n");
	connection->live = 0;
	connection->size = 0;
	connection->buffer[0]='\0';
	closesocket(connection->socket);
	sem_post( &connection->mutex );
}


void * server_socket_thread(void * data)
{
	int port = *(int*)data;
	SOCKET server, client;
 
	SOCKADDR_IN serverAddr, clientAddr;
 
	server = socket(AF_INET, SOCK_STREAM, 0);
 
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
 
	bind(server, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
	listen(server, 0);

	int clientAddrSize = sizeof(clientAddr);
	for(;;)
	if((client = accept(server, (SOCKADDR *)&clientAddr, &clientAddrSize)) != INVALID_SOCKET)
	{	
		int port = 0;
		recv(client,(char*)&port,sizeof(int),0);

		if(port>0)
		for(int i = 0; i < MAX_CONNECTIONS; i++)
		{
			sem_wait(&connections[i].mutex);
			if( connections[i].live == 0)
			{
				printf("Received connection on port %d %d!\n",port,ntohs(clientAddr.sin_port));
				connections[i].socket = client;
				connections[i].addr = clientAddr;
				connections[i].live = 1;
				connections[i].port = port;// ntohs(clientAddr.sin_port);
				
				pthread_create(&connections[i].thread, NULL, client_socket_thread, &connections[i]);
				// thread will post
				break;
			}
			sem_post(&connections[i].mutex);
		}

	}	
}

void connection_send(CONNECTION * connection, char * data, int size)
{
	sem_wait( &connection->mutex );
	if(connection->live)
	{
		sem_post( &connection->mutex );
		send(connection->socket,data,size,0);
	}
	else sem_post( &connection->mutex );
}

void connection_read(CONNECTION * connection, char * data, int size)
{
	sem_wait( &connection->mutex );
	if(connection->live)
	{
		if(connection->buffer[0]=='\0') memset(data,0,size);
		else
		{
			memcpy(data,connection->buffer,size);
			connection->buffer[0] = '\0';
		}
	}
	else memset(data,0,size);
	sem_post( &connection->mutex );
}

CONNECTION * ping(int server_port, int port)
{
	if(server_port == port) return NULL;
	for(int i = 0; i < MAX_CONNECTIONS; i++)
	{
		sem_wait(&connections[i].mutex);
		if(connections[i].live)
		if(connections[i].port==port)
		{
			sem_post(&connections[i].mutex);
			return &connections[i];
		}
		sem_post(&connections[i].mutex);
	}
	
	for(int i = 0; i < MAX_CONNECTIONS; i++)
	{
		sem_wait(&connections[i].mutex);
		if(connections[i].live==0)
		{
			SOCKET server;
			SOCKADDR_IN addr;

			server = socket(AF_INET, SOCK_STREAM, 0);
		 
			addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			printf("Connecting to %d\n", htons(port));
		 
			int r = connect(server, (SOCKADDR *)&addr, sizeof(addr));
			send(server,(char*)&server_port,sizeof(int),0);
			
			if(r) { printf("ERROR!\n"); sem_post(&connections[i].mutex); break; }
			else printf( "Connected to server!\n");
			
			connections[i].port = port;
			connections[i].socket = server;
			connections[i].addr = addr;
			connections[i].live = 1;
			connections[i].size = 0;
			
			pthread_create(&connections[i].thread, NULL, client_socket_thread, &connections[i]);
			
			return &connections[i];
		}
		sem_post(&connections[i].mutex);
	}
	
	return NULL;
}

int main(int argc, char **argv) 
{
	if(argc < 2)
	{
		printf("Please supply a port number.\n");
		return 0;
	}
	
	int server_port = atoi(argv[1]);
	
	WSAStartup(MAKEWORD(2,0), &WSAData);
	
	for(int i = 0; i < MAX_CONNECTIONS; i++)
	{
		sem_init(&connections[i].mutex,0,1);
		sem_init(&connections[i].empty,0,1);
	}
	
	pthread_create(&server_thread, NULL, server_socket_thread, &server_port);
	
	int waiting = 1,quit=0;
	
	
	NODE node = {0};
	node.info.port = server_port;
	
	HASH_ENTRY tmp = {{},(char*)&server_port,4}; get_hash(&tmp);
	memcpy(node.info.id,tmp.hash,sizeof(K_ID));
	printf("Your node ID for port %d is: ", server_port); hash_print(node.info.id); printf("\n");
	
	if(argc > 2)
	{
		int port = atoi(argv[2]);
		
		CONTACT tmpc; tmpc.port = port;
		if(port)
		{			
			rpc_ping(&node,&tmpc);//ping(server_port,port);
			Sleep(10); // windows function
			kademlia_find_value(&node,&tmp); // populate routing table
		}
	}
	
	for(;!quit;)
	{
		GENERIC_MESSAGE message = {NO_MESSAGE};
		char * buffer = message.buffer;
		
		// check for new messages and print
		
		if (_kbhit()) waiting=0; // windows function
		
		for(int i = 0; i < MAX_CONNECTIONS; i++)
		{
			GENERIC_MESSAGE tmp;
			connection_read(&connections[i],(char*)&tmp,sizeof(GENERIC_MESSAGE));
			
			switch(tmp.type)
			{
				#define CASE(X) case X: printf("received %s from connection %d:\n",#X,i);
				
				CASE(TEXT_MESSAGE)
				{
					printf("%s\n",tmp.buffer);
				}
				break;
				CASE(RPC_REQUEST)
				{
					printf("type=%d, data payload=%d\n",tmp.rpc.type,tmp.rpc.data_size);
					read_rpc(&node,&connections[i],&tmp.rpc);
				}
				break;
			}
		}
		
		buffer[0] = '\0';
		message.type = NO_MESSAGE;
		
		if(waiting) continue;
		
		gets(buffer);
		
		if(buffer[0] == '/')
		{
			char * dlm = " \t\n";
			char * tok = strtok(buffer, dlm);
			if(strcmp("/ping",tok)==0)
			{
				char * v = strtok(NULL,dlm);
				int port = atoi(v?v:"0");
				printf("attempting to connection on %d\n", port);
				CONTACT tmp; tmp.port = port;
				if(port) rpc_ping(&node,&tmp);
			}
			else if(strcmp("/wait",tok)==0) waiting=1; // do not poll for input
			else if(strcmp("/save",tok)==0)
			{
				HASH_ENTRY entry = {0};
				
				entry.size = strlen(tok + 6);
				entry.data = (char*)malloc(entry.size+1); memcpy(entry.data,tok+6,entry.size);
				entry.data[entry.size] = '\0';
				
				
				get_hash(&entry);
				
				printf("adding hash: "); hash_print(entry.hash); printf("\n");
				printf("DATA: %s\n",entry.data);
				hash_insert(node.table,&entry);
			}
			else if(strcmp("/load",tok)==0)
			{
				char * dlm = " \t\n";
				char * id_t = strtok(NULL, dlm);
				
				HASH_ENTRY entry={0};
				if(id_t)
				{
					for(int i = 0; id_t[i] && i < 2*sizeof(K_ID); i++)
					{
						char tmp[3] = {id_t[i*2 + 0],id_t[i*2 + 1],0};
						entry.hash[i] = strtol(tmp,NULL,16);
					}
				}
				
				printf("looking for hash: "); hash_print(entry.hash); printf("\n");

				hash_search(node.table,&entry);
				
				if(entry.data)
				{
					printf("DATA: %s\n",entry.data);
				}
				else printf("Nothing found!\n");
			}
			else if(strcmp("/send",tok)==0)
			{
				HASH_ENTRY entry = {0};
				
				entry.size = strlen(tok + 6)+1;
				entry.data = (char*)malloc(entry.size); memcpy(entry.data,tok+6,entry.size);
				entry.data[entry.size] = '\0';
				
				
				get_hash(&entry);
				
				printf("sending hash: "); hash_print(entry.hash); printf("\n");
				printf("DATA: %s\n",entry.data);
				//hash_insert(node.table,&entry);
				
				RPC_MESSAGE message = {STORE};
				message.entry = entry;
				
				printf("size: %d\n", message.entry.size); 
				for(int i = 0; i < MAX_CONNECTIONS; i++)
					send_rpc(&node,&connections[i],&message);
			}
			else if(strcmp("/init",tok)==0); // initialize chat state
			else if(strcmp("/quit",tok)==0) quit=1; // initialize chat state
			_kbhit(); // windows function
			waiting = 1;
			
		}
		else
		{
			// general post to chat
			
			message.type = TEXT_MESSAGE;
			for(int i = 0; i < MAX_CONNECTIONS; i++)
			{
				connection_send(&connections[i],(char*)&message,sizeof(GENERIC_MESSAGE));
			}
			_kbhit(); // windows function
			waiting = 1;
		}

	}

	for(int i = 0; i < MAX_CONNECTIONS; i++)
	{
		sem_wait(&connections[i].mutex);
		if(connections[i].live)
		{
			closesocket(connections[i].socket);
			connections[i].live = 0;
		}
		sem_post(&connections[i].mutex);
	}
	
	WSACleanup();
	
    return 0;
}