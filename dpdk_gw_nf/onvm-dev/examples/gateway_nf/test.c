#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_log.h>

#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "test"

#define MAX_PKT_BURST 32
#define EXPIRE_TIME 10000000

typedef enum { false=0, true } bool;

uint64_t p_info;
bool keep_running = true;

typedef uint64_t word_t;

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
	Conn *connection;
	bool drop_packet;
} Record;

// static void
// do_stats_display();

/* print packet receipt */
static void
do_stats_display(void)
{
	const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};

        /* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);
	printf("Packets recvd: %lu\n",p_info);
}

static int
handle_packet(struct rte_mbuf **pkts, uint16_t nb_recv)
{
	struct rte_mbuf *pkt;
	int i;
	static uint32_t counter = 0;

	if (++counter == EXPIRE_TIME) {
                do_stats_display();
                counter = 0;
        }

	for (i = 0; i < nb_recv; i++) {
		pkt = pkts[i];
		Conn *conn = (Conn *)malloc(sizeof(Conn));
		Record *rec = (Record *)malloc(sizeof(Record));
		if (!conn || !rec) return -1;

		struct ipv4_hdr *ip = onvm_pkt_ipv4_hdr(pkt);
		if (!ip) {
			RTE_LOG(CRIT, EAL, "Bad IP\n");
			return -1;
		}
		conn->dst_ip = ip->dst_addr;
		conn->src_ip = ip->src_addr;

		struct tcp_hdr *t_hdr = onvm_pkt_tcp_hdr(pkt);
		if (!t_hdr) {
			RTE_LOG(CRIT, EAL, "Bad TCP\n");
			return -1;
		}
		conn->ack_num = t_hdr->recv_ack;
		conn->dst_port = t_hdr->dst_port;
		conn->seq_num = t_hdr->sent_seq;
		conn->src_port = t_hdr->src_port;
		conn->rx_win = t_hdr->rx_win;

		rec->id = 1;
		rec->connection = conn;
		rec->drop_packet = false;
		rec->nf_map = 0;
	}

	return 0;
}

void
handle_kill_signal(int signal);

void
handle_kill_signal(__attribute__((unused))int signal)
{
	keep_running = false;
}

// static int
// callback_handler(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx);

int
main(int argc, char **argv)
{
	int arg_offset;
	struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        // const char *progname = argv[0];

	struct rte_mbuf *recv_pkts_burst[MAX_PKT_BURST];
	struct rte_ring *rx;
	// struct rte_ring *tx;

	uint16_t nb_recv;

	nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

	nf_function_table = onvm_nflib_init_nf_function_table();
        // nf_function_table->pkt_handler = &packet_handler;
        // nf_function_table->user_actions = &callback_handler;


        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

	argc -= arg_offset;
        argv += arg_offset;

	signal(SIGINT, handle_kill_signal);

	/* get the pointer to the rings */
	rx = nf_local_ctx->nf->rx_q;
	// tx = nf_local_ctx->nf->tx_q;

	/* indicate to mgr that we are ready to receive packets */
	onvm_nflib_nf_ready(nf_local_ctx->nf);

	while (keep_running) {
		/* get packets */
		nb_recv = rte_ring_dequeue_burst(rx, (void **)recv_pkts_burst, MAX_PKT_BURST, NULL);

		if (nb_recv > 0) {
			p_info += nb_recv;
			handle_packet(recv_pkts_burst, nb_recv);
			/* send the packets back */
			onvm_pkt_process_tx_batch(nf_local_ctx->nf->nf_tx_mgr, recv_pkts_burst, nb_recv, nf_local_ctx->nf);
			onvm_pkt_flush_all_nfs(nf_local_ctx->nf->nf_tx_mgr, nf_local_ctx->nf);
		}
	}

	onvm_nflib_stop(nf_local_ctx);
	return 0;
}