#ifndef DHT_H
#define DHT_H

//
//		This implementation of a DHT protocol is based on Kademlia:
//		http://xlattice.sourceforge.net/components/protocol/kademlia/specs.html
//

// Kademlia is parameterized by alpha, B, and k:
// 	N_CONTACTS is K=20 in the kademlia spec
// 	K_ID_LEN is (B=160)/8 (the number of bytes in a hash id)
//	PARRALLEL_QUERIES is alpha=3 in the spec

//	alpha controls how many peers are queried in parallel for the lookup operations
// 	we are using alpha = 1 for simplicity of implementation.


#include "connection.h"

#define PARALLEL_QUERIES 1
#define N_CONTACTS 5
#define K_ID_LEN 20
#define N_NODES 64
#define MAX_CONTACTS (N_NODES*4)
#define N_REPLACEMENTS 20

typedef unsigned char K_ID[K_ID_LEN]; // a 160 bit value

typedef struct
{
	K_ID hash;
	char * data;
	int size;
}HASH_ENTRY;

typedef HASH_ENTRY HASH_TABLE[0xFFFF];


typedef struct
{
	CONNECTION * connection;
	K_ID id;
	unsigned ip; // use ip in a real networked implementation
	unsigned port;
	//unsigned idx; // this is for our simulation only
	unsigned last_seen;
	int is_online;
	
	// This is also for simulation
	// we won't be able to share this value
	// in a true implementation
	//K_ID distance;
	
} CONTACT;

struct BUCKET_TREE_t; typedef struct BUCKET_TREE_t BUCKET_TREE;

typedef struct BUCKET_t
{
	BUCKET_TREE * parent;	
	CONTACT * contacts[N_CONTACTS];
	int n_contacts;
	int circle_idx;
	//CONTACT * replacements;
	//int n_replacements;
	
} BUCKET;

struct BUCKET_TREE_t
{
	K_ID min,max;
	//BUCKET_TREE_t * parent;
	BUCKET_TREE_t * children[2];
	BUCKET * bucket;
};

typedef struct
{
	CONTACT info;
	HASH_TABLE table;
	BUCKET_TREE * contacts;
	int is_online;
	CONTACT contact_table[MAX_CONTACTS]; 
} NODE;

//
//		RPC Protocol
//

enum RPCS
{
	FAILURE, PING, STORE, FIND_NODE, FIND_VALUE, // requests
	FOUND_NODE, FOUND_VALUE, // response
};

enum MESSAGES
{
	NO_MESSAGE,
	RPC_REQUEST, RPC_RESPONSE,
	TEXT_MESSAGE,
};

typedef struct
{
	int type;
	CONTACT sender;
	HASH_ENTRY entry;
	CONTACT closest[N_CONTACTS];
	
	int data_size;
	char * data;
} RPC_MESSAGE;

typedef struct
{
	int type,length;
	union
	{
		RPC_MESSAGE rpc;
		char buffer[SOCKET_BUFFER_SIZE - 128]; //-128 header space
	};
} GENERIC_MESSAGE;

//
//	Node functions
//


RPC_MESSAGE send_rpc(NODE * node, CONNECTION * connection, RPC_MESSAGE * input);
RPC_MESSAGE read_rpc(NODE * node, CONNECTION * connection, RPC_MESSAGE * input);

void kademlia_search(NODE * node, K_ID hash, HASH_ENTRY * entry, CONTACT ** closest, CONTACT ** exclusion, int * n_exclusion);
void kademlia_store_value(NODE * node, HASH_ENTRY * entry);
void kademlia_find_value(NODE * node, HASH_ENTRY * entry);

CONTACT * rpc_ping(NODE * sender, CONTACT * contact);

void get_hash(HASH_ENTRY * entry);
void hash_print(K_ID hash);


void hash_insert(HASH_TABLE table, HASH_ENTRY * entry);
void hash_search(HASH_TABLE table, HASH_ENTRY * entry);

#endif