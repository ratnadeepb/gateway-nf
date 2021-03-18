#ifndef __OPS_H__
#define __OPS_H__

#include "includes.h"

/* add header to the db */
// the signature would be later changed to the next line
// void add_record(struct rte_mbuf *pkt);
// for now, we will test with this one
void add_record(int16_t rx_win,
	uint32_t seq_num,
	uint32_t ack_num,
	uint32_t dst_ip,
	uint32_t src_ip,
	uint16_t dst_port,
	uint16_t src_port);

/* remove header from the completed db */
void remove_record(uint8_t id);

/* migrate headers from processing to completed db */
void migrate_record(uint8_t id);

/* receive data from NF and use it to modify existing headers
 * headers in the completed region should never be modified
 */
void modify_record(uint8_t id);

#endif