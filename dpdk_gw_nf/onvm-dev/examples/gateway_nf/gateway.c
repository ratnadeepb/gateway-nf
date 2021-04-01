#include "nf_api.h"


/* create a new record from an rte_mbuf */
// Record new_record(struct rte_mbuf);
// for now we use this
Record new_record(int16_t rx_win, uint32_t seq_num, uint32_t ack_num, uint32_t dst_ip, uint32_t src_ip, uint16_t dst_port, uint16_t src_port);

void
handle_kill_signal(int signal)
{
	keep_running = false;
}

/* store the pid of the gateway in a /run file
 * this is how the NFs know the pid of the NF
 * and send a signal to it for registration
 */
bool
store_pid()
{
	printf("Gateway NF PID: %d\n", getpid());
	int pid_f = open(PID_FILE, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (pid_f == -1) {
		perror("error opening pid file");
		return false;
	}
	char *buf = (char *)malloc(sizeof(int));
	sprintf(buf, "%d", getpid());
	if (write(pid_f, buf, strlen(buf)) == -1) {
		perror("error writing pid");
		return false;
	}
	close(pid_f);
	return true;
}

/* create the file and a stream off it for NFs to register */
void
create_nf_tag_list_file()
{
	creat(NF_TAG_FILE, O_CREAT | S_IRUSR | S_IWRITE | S_IWGRP | S_IRGRP | S_IROTH | S_IWOTH);
	/* gateway only reads from this file */
	NF_TAG_STRM = fopen(NF_TAG_FILE, "r");
}

/* create a new memory segment for a connection and return the corresponding entry struct */
GEntry
create_gw_region(char *conn_name)
{
	/* create a new shared file system name for an incoming connection */
	char *file_name = (char *)malloc(strlen(CONNECTION_BASE_NAME) + strlen(conn_name) + 1);
	strcpy(file_name, CONNECTION_BASE_NAME);
	strcat(file_name, conn_name);

	printf("file name: %s\n", file_name);

	int fd = open(file_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd == -1) {
		perror("error opening file");
		return GDEF;
	}
	
	if (ftruncate(fd, NUM_SLOTS * sizeof(Record)) != 0) {
		perror("truncate failure");
		return GDEF;
	}

	struct stat sb;
	Record *mem;
	if (fstat(fd, &sb) == 0) {
		mem = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mem == MAP_FAILED) {
			perror("mapping failed");
			return GDEF;
		}
	}

	GEntry res = {
		.entry = {
			.file_name = file_name,
			.mem = mem,
			.head = 0,
			.tail = 0,
		},
		.completed_tail = 0,
	};
	return res;
}

/* increment a pointer in a connection memory segment */
off_t
incr_record_ptr(off_t ptr)
{
	return (ptr + 1) % NUM_SLOTS;
}

/* check if there is place for one more record for the given connection */
bool
is_record_queue_free(GEntry *conn)
{
	off_t nxt_head = incr_record_ptr(conn->entry.head);
	if (nxt_head == conn->completed_tail) return false;
	return true;
}

/* add a new connection to the system */
bool
add_connections(GEntry new_conn)
{
	int pos;
	for (pos = 0; pos < NUM_MAX_CONNS; pos++) {
		GEntry *conn = &CONN_LIST[pos];
		if (!conn->entry.file_name) {
			CONN_LIST[pos] = new_conn;
			return true;
		}
	}
	return false;
}

/* search through the connection list
 * conn_hash: `conn_<hash_value>
 */
GEntry *
lookup_connection(char *conn_hash)
{
	size_t sz = strlen(CONNECTION_BASE_NAME) + strlen(conn_hash) + 1;
	char *query = (char *)malloc(sz);
	memset(query, 0, sz);
	strncpy(query, CONNECTION_BASE_NAME, strlen(CONNECTION_BASE_NAME));
	strcat(query, conn_hash);
	int pos;
	for (pos = 0; pos < NUM_MAX_CONNS; pos++) {
		GEntry *conn = &CONN_LIST[pos];
		if (!conn->entry.file_name) break;
		if (strncmp(conn->entry.file_name, query, strlen(query)) == 0)
			return &CONN_LIST[pos];
	}
	return NULL;
}

#define HASH_KEY 0x0f

/* create the hash of a connection */
uint32_t
record_hash(Record *rec)
{
	uint32_t res = rec->connection.dst_ip ^ rec->connection.src_ip
	^ rec->connection.dst_port ^ rec->connection.src_port;
	return res ^ HASH_KEY;
}

/* allocate ID for a new record and update the Redis key as well */
uint64_t
allocate_id_n_update_redis(uint32_t hash)
{
	char *query = (char *)malloc(strlen("conn_") + sizeof(uint32_t) + 1);
	strncpy(query, "conn_", strlen("conn_"));
	char *h = (char *)malloc(sizeof(uint32_t) + 1);
	sprintf(h, "%"PRIu32, hash);
	strcat(query, h);
	uint64_t id = 0;
	uint64_t ret = redis_get_latest_rec(query);
	if (ret == 0) {
		/* this is a new connection */
		srand(time(NULL));
		GEntry new_conn = create_gw_region(query); /* create a new memory region for the new conn */
		add_connections(new_conn); /* add to known connections */
		uint64_t id = rand(); /* create a new id */
		redis_add_latest_rec(query, id); /* add connection to redis */
		/* add to global map of connections */
	} else {
		id = ret++; /* in Ubuntu this wraps around */
		redis_add_latest_rec(query, id); /* update redis */
	}
	return id;
}

/* sets errno if out of memory */
void
add_record_to_file(uint32_t hash, Record *rec)
{
	size_t sz = strlen("conn_") + sizeof(uint32_t) + 1;
	char *query = (char *)malloc(sz);
	memset(query, 0, sz);
	strncpy(query, "conn_", strlen("conn_"));
	char *h = (char *)malloc(sizeof(uint32_t) + 1);
	sprintf(h, "%"PRIu32, hash);
	strcat(query, h);
	GEntry *conn = lookup_connection(query);
	if (!conn) {
		errno = ENOMEM;
		return;
	}
	if (is_record_queue_free(conn)) {
		off_t nxt_head = incr_record_ptr(conn->entry.head);
		Record *loc = conn->entry.mem + nxt_head;
		memmove((void *)loc, (void *)rec, sizeof(Record));
	} else {
		errno = ENOMEM;
	}
}

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
	uint16_t src_port)
{
	Record rec = {
		.id = 0,
		.nf_map = 0,
		.connection = {
			.rx_win = rx_win,
			.seq_num = seq_num,
			.ack_num = ack_num,
			.dst_ip = dst_ip,
			.src_ip = src_ip,
			.dst_port = dst_port,
			.src_port = src_port,
		},
		.drop_packet = false,
	};
	uint32_t hash = record_hash(&rec);
	rec.id = allocate_id_n_update_redis(hash);
	add_record_to_file(hash, &rec);
}

/* register a new NF to the system */
bool
register_nf(char *nf_name)
{
	int i;
	/* TODO: we should check if the same name already exists */
	int pos = first_free_bit(&GLOBAL_BITMAP);
	if (pos == -1) {
		printf("can't add anymore NFs\n");
		return false;
	}
	set_bit(&GLOBAL_BITMAP, pos);
	printf("pos: %d\n", pos);
	strncpy(NF_LIST[pos], nf_name, strlen(nf_name));
	printf("the bitmap: %"PRIu64"\n", GLOBAL_BITMAP);
	for (i = 0; i <= pos; i++) printf("the nf list: %s\n", NF_LIST[i]);
	return true;
}

/* NFs register with gateway by sending SIGUSR1
 * they append a name tag into the nf tag file
 * upon receiving SIGUSR1, the gateway reads the file and gets the new tag and registers the NF
 */
void
handle_user_signals(int signal)
{
	switch (signal)
	{
	case SIGUSR1:
		printf("siguser1\n");
		char *nf_tag = (char *)malloc(sizeof(30));
		while (fgets(nf_tag, 30, NF_TAG_STRM)) {
			printf("from nf tag stream: %s\n", nf_tag);
			register_nf(nf_tag);
			memset(nf_tag, 0, 30);
		}
		break;
	default:
		break;
	}
}



/* main work loop
 * receive packets
 * create records
 * and update redis
 */
static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx)
{
	printf("run loop\n");
	while (keep_running) {}
	printf("breaking out of the run loop\n");
	return NULL;
}

/*
 * Prints out information about flows stored in table
 * TODO: Change this
 */
static void
do_stats_display(struct state_info *state_info) {
        struct flow_stats *data = NULL;
        struct onvm_ft_ipv4_5tuple *key = NULL;
        uint32_t next = 0;
        int32_t index;

        printf("------------------------------\n");
        printf("     Flow Table Contents\n");
        printf("------------------------------\n");
        printf("Current capacity: %d / %d\n\n", state_info->num_stored, TBL_SIZE);
        while ((index = onvm_ft_iterate(state_info->ft, (const void **)&key, (void **)&data, &next)) > -1) {
                update_status(state_info->elapsed_cycles, data);
                printf("%d. Status: ", index);
                if (data->is_active) {
                        printf("Active\n");
                } else {
                        printf("Expired\n");
                }

                printf("Key information:\n");
                _onvm_ft_print_key(key);
                printf("Packet count: %d\n\n", data->pkt_count);
        }
}


static int
callback_handler(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        state_info->elapsed_cycles = rte_get_tsc_cycles();

        if ((state_info->elapsed_cycles - state_info->last_cycles) / rte_get_timer_hz() > state_info->print_delay) {
                state_info->last_cycles = state_info->elapsed_cycles;
                do_stats_display(state_info);
        }

        return 0;
}

int
main(int argc, char *argv[])
{
	int arg_offset;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        const char *progname = argv[0];

	nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->user_actions = &callback_handler;

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

	state_info = rte_calloc("state", 1, sizeof(struct state_info), 0);
        if (state_info == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF state");
        }

        state_info->print_delay = 5;
        state_info->num_stored = 0;

	onvm_nflib_run(nf_local_ctx);

	/* store the pid */
	store_pid();
	/* open the nf_tag_file */
	create_nf_tag_list_file();

	RCONTEXT = redisConnect("127.0.0.1", 6379);
	if (RCONTEXT != NULL && RCONTEXT->err) printf("Error %s\n", RCONTEXT->errstr);
	else printf("connected to redis\n");

	signal(SIGUSR1, handle_user_signals);
	signal(SIGKILL, handle_kill_signal);
	signal(SIGQUIT, handle_kill_signal);
	signal(SIGINT, handle_kill_signal);
}