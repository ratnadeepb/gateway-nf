/* 
 * This is the public API for the system
 */

#ifndef __INCLUDES_H__
#define __INCLUDES_H__

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

#include "bitmap.h"

typedef enum { false=0, true } bool;

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

/* used to point to the data of a connection */
typedef struct entry {
	Record *mem; /* shared memory file mapped to virtual memory */
	off_t off; /* offset to the start of the connection's location */
} Entry;

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
int register_nf(char *nf_name);


/* calculate the hash of a record */
uint32_t record_hash(Record *rec);

#endif