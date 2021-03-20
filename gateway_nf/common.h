#pragma once

#include "hiredis.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PID_FILE "/run/onvm_gateway_nf.pid" /* our pid file */
#define NF_TAG_FILE "/run/onvm_nf_tags" /* list of all NFs registering with the gateway */

#define NUM_SLOTS 512   /* each slot corresponds to a header */

#define MAX_NF_TAG_SZ 30

const char CONNECTION_BASE_NAME[] = "/dev/shm/conn_";

typedef enum { false=0, true } bool;

/* a 64 bit bitmap
 * this means we can support up to 64 NFs at any time
 * but this should be easily extensible
 */
typedef uint64_t word_t;
enum { BITS_PER_WORD = sizeof(word_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

void
set_bit(word_t *words, int n)
{
	words[WORD_OFFSET(n)] |= ((word_t)1 << BIT_OFFSET(n));
}

void
clear_bit(word_t *words, int n)
{
	words[WORD_OFFSET(n)] &= ~((word_t)1 << BIT_OFFSET(n));
}

int
get_bit(word_t *words, int n)
{
	word_t bit = words[WORD_OFFSET(n)] & ((word_t)1 << BIT_OFFSET(n));
	return bit != 0;
}

int
first_free_bit(word_t *word)
{
	int i;
	for (i = 0; i < BITS_PER_WORD; i++) {
		if (get_bit(word, i) == 0) return i;
	}
	return -1;
}

typedef struct Conn {
	int16_t rx_win;
	uint32_t seq_num;
	uint32_t ack_num;
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t dst_port;
	uint16_t src_port;
} Conn;

typedef struct Record {
	uint32_t id; /* to be used to find the rte_mbuf */
	word_t nf_map; /* the bitmap tell which NFs have already finished processing */
	Conn connection;
	bool drop_packet;
} Record;

/* create a new record from an rte_mbuf */
// Record new_record(struct rte_mbuf);
// for now we use this
Record new_record(int16_t rx_win, uint32_t seq_num, uint32_t ack_num, uint32_t dst_ip, uint32_t src_ip, uint16_t dst_port, uint16_t src_port);

/* used to point to the data of a connection
 * every NF should hold this data separately
 * since every NF can be at different stages of processing
 * the gateway nf advances the tail when a brand new packet arrives
 * the gateway nf advances the head only when all NFs have processed the head packet
 * 
 * packets are not dropped when the head advances
 * the packets that have been processed but not dropped are completed packets
 * they are held onto till the backend application responds to these packets
 * these packets will allow the system to fail a connection over
 * since it still holds all unacknowledged packets the connection can be replyed at another backend
 */
typedef struct entry {
	char *file_name; /* name of the file that's mapped */
	Record *mem; /* shared memory file mapped to virtual memory */
	off_t head; /* offset to the first packet */
	off_t tail; /* offset into the memory where the last packet is */
} Entry;

typedef struct getway_entry {
	Entry entry;
	off_t completed_tail; /* track the processed records corresponding to unacknowledge packets */
} GEntry;

/* Default unuseable values for Entry and GEntry
 * Should be used to signal error
 */
// const Entry EDEF = {.file_name = "", .mem = NULL, .head = -1, .tail = -1};
const GEntry GDEF = {
	.entry = {
		.file_name = "",
		.mem = NULL,
		.head = -1,
		.tail = -1,
	},
	.completed_tail = -1,
};

/* compare the equality of two Entry or GEntry structs by value */
bool compare_entry(const Entry *lhs, const Entry *rhs);
bool compare_gentry(const GEntry *lhs, const GEntry *rhs);

/* every connection has its own file and associated file name
 * the redis db maintains the connection's state
 * the connection's state is simply the last packet id in the system for that connection
 * when this update happens, redis informs all subscribers
 * subscribing NFs then can access the shared file to process the latest headers
 */
bool redis_update_conn(char *file_name, uint32_t id);

/* 
 * copy_record copies a record to a memory location provided by the client
 * This function is at the core of the CoW design
 * @rec -> original pointer that is to be copied
 * @mem -> head of the memory region to hold the records queue
 * @offset -> where in the queue the new record needs to be stored at
 */
void copy_record(Record *rec, Record *mem, off_t offset)
{
	Record *ptr = mem + offset;
	memcpy(ptr, rec, sizeof(Record));
}

/* register an NF
 * returns an ID for the NF
 */
bool register_nf(char *nf_name);

/* create a memory region for a new incoming connection
 * returns a gateway entry struct
 * there will be a corresponding `create_nf_region(char *conn_name)` for NFs
 * that will return an Entry struct
 */
GEntry create_gw_region(char *conn_name);

/* add record to redis db
 * conn_name is the identifier for a connection
 * id is the id of the latest record added to this connection
 */
bool redis_add(char *conn_name, uint64_t id);

/* get record from the redis db
 * get the latest record id for the given connection
 */
uint64_t redis_get_record_id(char *conn_name);