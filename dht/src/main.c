#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include "stdint.h"

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;
uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

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

#define PARALLEL_QUERIES 1
#define N_CONTACTS 5
#define K_ID_LEN 20
#define N_NODES 64
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
	K_ID id;
	unsigned ip; // use ip in a real networked implementation
	unsigned idx; // this is for our simulation only
	unsigned last_seen;
	
	// This is also for simulation
	// we won't be able to share this value
	// in a true implementation
	K_ID distance;
	
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
} NODE;

NODE all_nodes[N_NODES]; // for our simulation

void hash_distance(K_ID a, K_ID b, K_ID out)
{
	for(int i = 0; i < K_ID_LEN; i++)
		out[i] = a[i]^b[i];
}

int hash_equ(K_ID a, K_ID b)
{
	for(int i = 0; i < K_ID_LEN; i++)
		if(a[i] != b[i]) return 0;
	return 1;
}

int hash_lth(K_ID a, K_ID b)
{
	for(int i = 0; i < K_ID_LEN; i++)
		if(a[i] < b[i]) return 1;
		else if(a[i] > b[i]) return 0;
	return 0;
}

int hash_in_range(K_ID hash, K_ID min, K_ID max)
{
	return hash_lth(hash,max) && (hash_lth(min,hash) || hash_equ(hash,min));
}

void hash_print(K_ID hash)
{
	for(int i = 0; i < K_ID_LEN; i++) 
		printf("%02X",hash[i]);
}

void hash_split(K_ID min, K_ID max, K_ID split)
{
	// calculate a midpoint of two hashes
	// ( find difference, divide by 2, add min)
	memset(split,0,sizeof(K_ID));
	
	unsigned carry = 1;
	for(int i = K_ID_LEN-1; i >= 0; i--)
	{
		unsigned a = max[i];
		unsigned b = (~min[i])&0xFF;
		split[i] = (a+b+carry)&0xFF;
		carry = ((a+b+carry)&0xFF00) >> 8;
	}
	
	carry = 0;
	for(int i = 0; i < K_ID_LEN; i++)
	{
		unsigned a = split[i];
		split[i] = (carry<<7) | (a>>1);
		carry = a&0x1;
	}
	
	carry = 0;
	for(int i = K_ID_LEN-1; i >= 0; i--)
	{
		unsigned a = split[i];
		unsigned b = min[i];
		split[i] = (a+b+carry)&0xFF;
		carry = ((a+b+carry)&0xFF00) >> 8;
	}	
}

void get_hash(HASH_ENTRY * entry)
{
	// No guarentee that this is a good hash, but it should be 
	// evenly distributed enough for our purposes.
	char * input = entry->data;
	int size = entry->size;
	unsigned * output = (unsigned *)entry->hash;
	
	// cryptographic salt for small inputs
	// just to help with evening the distribution
	unsigned buffer[4] = {0xFFAB10FA,0xA123ACDB,0x7FACE134,0xF0BC1724};
	
	if(size < 16) 
	{
		input = (char*)buffer;
		memcpy(buffer,entry->data,size);
		size = 16;
	}
	
	memset(entry->hash,0,sizeof(K_ID));
	
	unsigned remainder = 0;
	for(int i=size-(size%4); i < size; i++)
		remainder = (remainder<<8) | (input[i]&0xFF);
	
	pcg32_random_t pcg = {remainder,1};
	for(int i = 0; i < K_ID_LEN/4; i++)
		output[i] ^= pcg32_random_r(&pcg);
	
	for(int i = 0; i < size/4; i++)
	{
		pcg.state ^= ((unsigned*)input)[i];
		for(int j = 0; j < K_ID_LEN/4; j++)
			output[j] ^= pcg32_random_r(&pcg);
	}
}

void hash_insert(HASH_TABLE table, HASH_ENTRY * entry)
{
	// these functions will fail on a full table, 
	// but we should never let it be full anway
	
	unsigned short idx = entry->hash[K_ID_LEN-2] | (entry->hash[K_ID_LEN-1]<<8);
	while( table[idx].data ) 
	{
		if( hash_equ(table[idx].hash,entry->hash) ) break;
		idx++;	
	}
	
	table[idx] = *entry;
}

void hash_search(HASH_TABLE table, HASH_ENTRY * entry)
{
	unsigned short idx = entry->hash[K_ID_LEN-2] | (entry->hash[K_ID_LEN-1]<<8);
	while( !hash_equ( table[idx].hash, entry->hash ) )
	{
		if(table[idx].data) idx++;
		else return;
	}
	*entry = table[idx];
}

void merge_contact_lists(CONTACT ** dst, CONTACT ** src, K_ID hash)
{	
	K_ID src_distance[N_CONTACTS] = {0};
	K_ID dst_distance[N_CONTACTS] = {0};

	for(int i=0; dst[i] && i<N_CONTACTS; i++)
		hash_distance(hash,dst[i]->id,dst_distance[i]);
	
	for(int i=0; src[i] && i<N_CONTACTS; i++)
		hash_distance(hash,src[i]->id,src_distance[i]);
	
	for(int i=0; src[i] && i<N_CONTACTS; i++)
	{
		CONTACT * contact = src[i];
		int skip = 0;
		for(int j = 0; dst[j] && j < N_CONTACTS; j++)
		if(contact->idx == dst[j]->idx)
			skip = 1;
		
		if(skip) continue;
		
		int add = 0;
		int j = 0;
		for(; dst[j] && j < N_CONTACTS; j++)		
		if(hash_lth(src_distance[i],dst_distance[j]))
		{
			for(int k = N_CONTACTS-1; k > j; k--)
				dst[k] = dst[k-1];
			dst[j] = contact;
			add=1;
			break;
		}
		
		if(!add && j < N_CONTACTS && !dst[j]) 
		{
			dst[j] = contact;
			add=1;
		}
	}
}

BUCKET * find_bucket(BUCKET_TREE * tree, K_ID id)
{
	if(!tree) return NULL;
	
	if(tree->children[0])
	{
		if(hash_lth(id,tree->children[0]->max))
			return find_bucket(tree->children[0],id);
		else
			return find_bucket(tree->children[1],id);
	}
	else return tree->bucket;
}

void add_contact(NODE * node, CONTACT * contact)
{
	if(contact->id==node->info.id) return;
	
	#define MALLOC_Z(S) (memset(malloc((S)),0,(S)))
	
	BUCKET * bucket = find_bucket(node->contacts,contact->id);
	
	
	if(bucket == NULL)
	{
		node->contacts = (BUCKET_TREE*) MALLOC_Z(sizeof(BUCKET_TREE));
		bucket = node->contacts->bucket = (BUCKET*) MALLOC_Z(sizeof(BUCKET));
		bucket->parent = node->contacts;
		memset(bucket->parent->min,0x00,sizeof(K_ID));
		memset(bucket->parent->max,0xFF,sizeof(K_ID));
	}
	
	for(int i = 0; i < bucket->n_contacts; i++)
	if(bucket->contacts[i]->idx == contact->idx)
		return;
	
	if(bucket->n_contacts < N_CONTACTS) bucket->n_contacts++;
	else if( hash_in_range(node->info.id, bucket->parent->min, bucket->parent->max) )
	{		
		BUCKET_TREE * tree = bucket->parent;
		
		K_ID split; hash_split(tree->min,tree->max,split);
		
		tree->children[0] = (BUCKET_TREE*) MALLOC_Z(sizeof(BUCKET_TREE));
		tree->children[1] = (BUCKET_TREE*) MALLOC_Z(sizeof(BUCKET_TREE));
		tree->children[0]->bucket = (BUCKET*) MALLOC_Z(sizeof(BUCKET));
		tree->children[1]->bucket = (BUCKET*) MALLOC_Z(sizeof(BUCKET));
		
		tree->children[0]->bucket->parent = tree->children[0];
		tree->children[1]->bucket->parent = tree->children[1];
		tree->bucket = NULL;		
		
		memcpy(tree->children[0]->min,bucket->parent->min,sizeof(K_ID));
		memcpy(tree->children[1]->max,bucket->parent->max,sizeof(K_ID));
		memcpy(tree->children[0]->max,split,sizeof(K_ID));
		memcpy(tree->children[1]->min,split,sizeof(K_ID));
		
		for(int i=0; i < bucket->n_contacts; i++)
		{
			BUCKET * dst = find_bucket(tree,bucket->contacts[i]->id);
			dst->contacts[dst->circle_idx++] = bucket->contacts[i];
			dst->n_contacts++;
		}
		
		free(bucket);
		bucket = find_bucket(tree,contact->id);
	}
		
	bucket->circle_idx %= N_CONTACTS;
	bucket->contacts[bucket->circle_idx++] = contact;
	bucket->circle_idx %= N_CONTACTS;
	#undef MALLOC_Z
}

void list_contacts(BUCKET_TREE * tree)
{
	if(tree->children[0])
	{
		list_contacts(tree->children[0]);
		list_contacts(tree->children[1]);
	}
	else
	{
		BUCKET * bucket = tree->bucket;
		
		printf("Tree Node: "); hash_print(tree->min); printf("-"); hash_print(tree->max); printf("\n");
		for(int i = 0; i < bucket->n_contacts; i++)
		{
			printf("\tContact %d: ", bucket->contacts[i]->idx); hash_print(bucket->contacts[i]->id); printf("\n");
		}
	}	
}

//
//		Remote Procedure Call (RPC) interface
//

int rpc_ping(NODE * sender, CONTACT * contact)
{	
	NODE * node = &all_nodes[contact->idx];
	if(node->is_online) 
	{
		contact->last_seen = time(NULL);
		add_contact(sender,contact);
		add_contact(node,&sender->info);
	}
	return node->is_online;
}

void rpc_store_value(NODE * sender, CONTACT * contact, HASH_ENTRY * entry)
{
	if(!rpc_ping(sender,contact)) return;
	NODE * node = &all_nodes[contact->idx];	
	hash_insert(node->table,entry);
}

void rpc_find_node(NODE * sender, CONTACT * contact, K_ID hash, CONTACT ** closest)
{
	// iterate on all buckets of contacts
	// to find the nodes closest to the desired hash
	
	if(!rpc_ping(sender,contact)) return;	
	NODE * node = &all_nodes[contact->idx];
	
	//printf("searching %d\n",node->info.idx);
	
	BUCKET_TREE * stack[160*2] = {node->contacts}; // n*2 should be big enough
	int stack_size=1;

	while(stack_size>0)
	{
		BUCKET_TREE * bucket_tree = stack[--stack_size];
		BUCKET * bucket = bucket_tree->bucket;
		if(bucket_tree->children[0]) stack[stack_size++] = bucket_tree->children[0];
		if(bucket_tree->children[1]) stack[stack_size++] = bucket_tree->children[1];
		
		
		if(bucket) merge_contact_lists(closest,bucket->contacts,hash);
	}
	/*
	for(int i = 0; closest[i] && i < N_CONTACTS; i++)
	{
		printf("\t %d ", closest[i]->idx); hash_print(closest[i]->id); printf("\n");
	}*/
}

void rpc_find_value(NODE * sender, CONTACT * contact, HASH_ENTRY * entry, CONTACT ** closest)
{
	if(!rpc_ping(sender,contact)) return;
	NODE * node = &all_nodes[contact->idx];
	
	hash_search(node->table,entry);
	
	if(!entry->data) rpc_find_node(sender,contact,entry->hash,closest);
}

//
//		Kademlia Operations
//

void kademlia_search(NODE * node, K_ID hash, HASH_ENTRY * entry, CONTACT ** closest, CONTACT ** exclusion, int * n_exclusion)
{
	if(!hash && !entry) return;
	else if(!hash) hash = entry->hash;
	rpc_find_node(node,&node->info,hash,closest);
	
	int query_count = 0;
	
	for(;;)
	{
		CONTACT * new_contacts[N_CONTACTS] = {NULL};
		int n_new_contacts = 0;
		for(int i = 0; closest[i] && i < N_CONTACTS; i++)
		{
			int excluded = 0;
			for(int j = 0; j < *n_exclusion; j++)
			if(exclusion[j]->idx == closest[i]->idx)
				{ excluded=1; break; }
			
			if(!excluded) 
			{
				new_contacts[n_new_contacts++] = closest[i];
				exclusion[(*n_exclusion)++] = closest[i];
			}
			
		}
		
		if(n_new_contacts==0) return;
		
		query_count += n_new_contacts;
		
		for(int i = 0; i<n_new_contacts; i++)
		{
			
			CONTACT * query[N_CONTACTS] = {0};
			
			if(entry)
			{
				rpc_find_value(node,new_contacts[i],entry,query);
				
				if(entry->data) 
				{
					if(i-1>=0) rpc_store_value(node,closest[i-1],entry);
					printf("Found closest nodes in %d queries\n",query_count);
					return;
				}
			}
			else 
				rpc_find_node(node,new_contacts[i],hash,query);
			
			merge_contact_lists(closest,query,hash);
		}
	}
	
	printf("Found closest nodes in %d queries\n",query_count);
}

void kademlia_store_value(NODE * node, HASH_ENTRY * entry)
{
	CONTACT * exclusion[N_NODES] = {&node->info};
	CONTACT * closest[N_CONTACTS] = {0};
	int n_exclusion=1;
	
	printf("finding nodes closest to "); hash_print(entry->hash); printf("\n");
	
	kademlia_search(node,entry->hash,NULL,closest,exclusion,&n_exclusion);
	for(int i = 0; closest[i] && i < N_CONTACTS; i++)
	{
		printf("Storing data to node %d: ",closest[i]->idx); hash_print(closest[i]->id); printf("\n");
		rpc_store_value(node,closest[i],entry);
	}
}

void kademlia_find_value(NODE * node, HASH_ENTRY * entry)
{
	CONTACT * exclusion[N_NODES] = {&node->info};
	CONTACT * closest[N_CONTACTS] = {0};
	int n_exclusion=1;
	
	//printf("searching for value for node %d\n", node->info.idx);
	kademlia_search(node,NULL,entry,closest,exclusion,&n_exclusion);
}

int main(int argc, char * argv[])
{
	
	static char buffer[0xFFF];
	setvbuf( stdout, buffer, _IOFBF, sizeof(buffer) );
	
	// Hash table tests:
	char str0[256] = "This a test #1";
	char str1[256] = "This a test #2";
	HASH_ENTRY a={{},str0,256}; get_hash(&a);
	HASH_ENTRY b={{},str1,256}; get_hash(&b);
	
	printf("Hashing: \"%s\" ", str0); hash_print(a.hash); printf("\n");
	printf("Hashing: \"%s\" ", str1); hash_print(b.hash); printf("\n");
	
	HASH_TABLE table;
	HASH_ENTRY c,d;
	memcpy(c.hash,a.hash,sizeof(K_ID));
	memcpy(d.hash,b.hash,sizeof(K_ID)); 
	
	hash_insert(table,&a);
	hash_insert(table,&b);
	hash_search(table,&c);
	hash_search(table,&d);
	
	printf("Retrieved: \"%s\" ", c.data); hash_print(c.hash); printf("\n");
	printf("Retrieved: \"%s\" ", d.data); hash_print(d.hash); printf("\n");
	
	{
		// Midpoint test
		K_ID a,b,c={0};
		
		memset(a,0x00,sizeof(K_ID));
		memset(b,0xFF,sizeof(K_ID));
		
		printf("MIN: "); hash_print(a); printf("\n");
		for(int i = 0; i < 160; i++)
		{
			hash_split(a,b,c);
			printf("MID: "); hash_print(c); printf("\n");
			memcpy(a,c,sizeof(K_ID));
		}
		printf("MAX: "); hash_print(b); printf("\n");
	}

	
	
	{
		for(int i = 0; i < N_NODES; i++)
		{
			NODE * node = &all_nodes[i];
			node->info.idx = i;
			HASH_ENTRY tmp = {{},(char*)&i,4};	get_hash(&tmp);
			memcpy(node->info.id,tmp.hash,sizeof(K_ID));
			printf("creating node %d with id: ", i); hash_print(tmp.hash); printf("\n");
			node->is_online = 1;
		}
		
		
		for(int i = 0; i < N_NODES; i++)
		{
			add_contact(&all_nodes[i],&all_nodes[(i+1)%N_NODES].info);
			add_contact(&all_nodes[i],&all_nodes[(i+N_NODES-1)%N_NODES].info);
		}
		for(int i = 1; i < N_NODES; i++)
			add_contact(&all_nodes[0],&all_nodes[(i)%N_NODES].info);
		
		printf("Node 0 contacts:"); hash_print(all_nodes[0].info.id); printf("\n");
		list_contacts(all_nodes[0].contacts);
		
		kademlia_store_value(&all_nodes[0],&a);
		kademlia_store_value(&all_nodes[0],&b);
		
		printf("Node 0 contacts:"); hash_print(all_nodes[0].info.id); printf("\n");
		list_contacts(all_nodes[0].contacts);
		
	
		fflush(stdout);
		printf("\n");
		for(int i = 0; i < 10; i++)
		{
			// populate routing tables by looking up non-existent data
			HASH_ENTRY search = {0};
			memcpy(search.hash, all_nodes[i].info.id, sizeof(K_ID));
			for(int j = 1; j < N_NODES; j++)
				kademlia_find_value(&all_nodes[j],&search);	
		}
		
		int node = 20;
		printf("Node %d contacts:",node); hash_print(all_nodes[node].info.id); printf("\n");
		list_contacts(all_nodes[node].contacts);
		
		HASH_ENTRY search = {0};
		memcpy(search.hash, &b, sizeof(K_ID));
		kademlia_find_value(&all_nodes[node],&search);
		
		if(search.data)
			printf("Data found: %.*s", search.size, (char*)search.data);
		else
			printf("Data not found!");


		char rand_data[1000][128];
		HASH_ENTRY hashes[1000];
		
		for(int i = 0; i < 1000; i++)
		{
			for(int j = 0; j < 128; j++) rand_data[i][j] = rand();
			
			hashes[i].data = rand_data[i];
			hashes[i].size = 128; 
			get_hash(&hashes[i]);
			
			printf("Storing data with hash "); hash_print(hashes[i].hash); printf("\n");
			kademlia_store_value(&all_nodes[rand()%N_NODES],&hashes[i]);
		}
		
		int failures = 0;
		
		for(int i = 0; i < 1000; i++)
		{	
			HASH_ENTRY tmp = hashes[i];
			tmp.data = NULL;
			kademlia_find_value(&all_nodes[rand()%N_NODES],&tmp);
			if(tmp.data == hashes[i].data)
			{
				printf("Found data with hash "); hash_print(hashes[i].hash); printf("\n");
			}
			else
			{
				failures++;
				printf("Could not find data with hash "); hash_print(hashes[i].hash); printf("\n");
			}
			
		}		
		printf("number of failures %d\n", failures);
		
	}
	
	fflush(stdout);
	
}