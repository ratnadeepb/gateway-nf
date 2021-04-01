#pragma once

#include "common.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <signal.h>
// #include <pthread.h>
#include <math.h>
#include <errno.h>
#include <rte_mbuf.h>
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "gateway"

#define NUM_MAX_CONNS 1024 /* handle at most 1024 simultaneous connections */
#define NUM_SLOTS 512   /* each slot corresponds to a header */

#define MAX_NF_TAG_SZ 30

const char CONNECTION_BASE_NAME[] = "/dev/shm/";

typedef uint64_t word_t;
enum { BITS_PER_WORD = sizeof(word_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

static redisContext *RCONTEXT; /* a global connection to the Redis DB */
/* the main thread keeps reading from this stream to check if a new NF has joined */
static FILE *NF_TAG_STRM;
/* user signal turns this falls and the program quits and cleans up */
static bool keep_running = true;

/* registry of all NFs */
static word_t GLOBAL_BITMAP = 0;

/* maintain a list of all registered NFs */
static char NF_LIST[sizeof(uint64_t)][MAX_NF_TAG_SZ];

/* maintain a list of all active connections */
static GEntry CONN_LIST[NUM_MAX_CONNS];


/* a 64 bit bitmap
 * this means we can support up to 64 NFs at any time
 * but this should be easily extensible
 */

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

typedef struct conn_ {
	int16_t rx_win;
	uint32_t seq_num;
	uint32_t ack_num;
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t dst_port;
	uint16_t src_port;
} Conn;

typedef struct record_ {
	uint32_t id; /* to be used to find the rte_mbuf */
	word_t nf_map; /* the bitmap tell which NFs have already finished processing */
	Conn connection;
	bool drop_packet;
} Record;

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
typedef struct entry_ {
	char *file_name; /* name of the file that's mapped */
	Record *mem; /* shared memory file mapped to virtual memory */
	off_t head; /* offset to the first packet */
	off_t tail; /* offset into the memory where the last packet is */
} Entry;

typedef struct getway_entry_ {
	Entry entry;
	off_t completed_tail; /* track the processed records corresponding to unacknowledge packets */
} GEntry;

/* Default unuseable values for GEntry
 * Should be used to signal error
 */
const GEntry GDEF = {
	.entry = {
		.file_name = NULL,
		.mem = NULL,
		.head = -1,
		.tail = -1,
	},
	.completed_tail = -1,
};

/* Compare two gateway entry structs */
bool
compare_gentry(const GEntry *lhs, const GEntry *rhs)
{
	/* it's enough to check that the Entry members are the same */
	return compare_entry(&lhs->entry, &rhs->entry);
}


/* APIs for nf */
/* ================================ */

/* create a new record from an rte_mbuf */
Record new_record(struct rte_mbuf);

/* every connection has its own file and associated file name
 * the redis db maintains the connection's state
 * the connection's state is simply the last packet id in the system for that connection
 * when this update happens, redis informs all subscribers
 * subscribing NFs then can access the shared file to process the latest headers
 */
bool redis_update_conn(char *file_name, uint32_t id);

/* create a new memory segment for a connection and return the corresponding entry struct */
GEntry
create_gw_region(char *conn_name);

/* increment a pointer in a connection memory segment */
off_t
incr_record_ptr(off_t ptr);

/* check if there is place for one more record for the given connection */
bool
is_record_queue_free(GEntry *conn);

/* add a new connection to the system */
bool
add_connections(GEntry new_conn);

/* search through the connection list
 * conn_hash: `conn_<hash_value>
 */
GEntry *
lookup_connection(char *conn_hash);

/* allocate ID for a new record and update the Redis key as well */
uint64_t
allocate_id_n_update_redis(uint32_t hash);

/* sets errno if out of memory */
void
add_record_to_file(uint32_t hash, Record *rec);

/* add a record to the connection's memory segment
 * might fail by setting errorno
 */
void
add_record(int16_t rx_win,
	uint32_t seq_num,
	uint32_t ack_num,
	uint32_t dst_ip,
	uint32_t src_ip,
	uint16_t dst_port,
	uint16_t src_port);


/* register a new NF to the system */
bool
register_nf(char *nf_name);
