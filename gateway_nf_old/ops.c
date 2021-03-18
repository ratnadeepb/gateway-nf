#include "ops.h"
#include <math.h>

#define HASH_KEY 0x0f /* should remain secret */
static uint64_t CURRENT_ID = 0; /* we can change this to some other scheme later */

uint32_t record_hash(Record *rec)
{
	uint32_t res = rec->connection.dst_ip ^ rec->connection.src_ip
	^ rec->connection.dst_port ^ rec->connection.src_port;
	return res ^ HASH_KEY;
}

/* create connection from mbuf */
// this would create a connection from a mbuf (later)
// Conn create_connection(struct rte_mbuf *pkt);

/* create a new record */
Record create_record(Conn connection)
{
	word_t nf_map = (uint64_t)0;
	bool drop_packet = false;
	uint32_t id = CURRENT_ID++;
	Record rec = {
		.id = id,
		.nf_map = nf_map,
		.connection = connection,
		.drop_packet = drop_packet,
	};
	return rec;
}

void add_record(int16_t rx_win,
	uint32_t seq_num,
	uint32_t ack_num,
	uint32_t dst_ip,
	uint32_t src_ip,
	uint16_t dst_port,
	uint16_t src_port)
{
	Conn connection = { .rx_win = rx_win,
		.seq_num = seq_num,
		.ack_num = ack_num,
		.dst_ip = dst_ip,
		.src_ip = src_ip,
		.dst_port = dst_port,
		.src_port = src_port,
		};
	
	Record rec = create_record(connection);
}