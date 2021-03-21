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
#include <pthread.h>
#include <math.h>
#include <errno.h>

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

/* add a record id to the connection key
 * conn_name: `conn_<hash_value>`
 */
bool
redis_add_latest_rec(char *conn_name, uint64_t id)
{
	bool res = false;

	redisReply *reply;

	char *val = (char *)malloc(sizeof(uint64_t) + 1);
	if (val == NULL) {
		perror("failed creating val");
		return EXIT_FAILURE;
	}
	
	sprintf(val, "%"PRIu64, id);

	/* build the whole command */
	char *cmd = (char *)malloc(strlen("set ") + strlen(conn_name) + strlen(" ") + strlen(val) + 1);
	strcpy(cmd, "set ");
	strcat(cmd, conn_name);
	strcat(cmd, " ");
	strcat(cmd, val);

	printf("in redis_add_latest_rec: cmd: %s\n", cmd); // DEBUG:

	reply = redisCommand(RCONTEXT, cmd);
	if ((int)reply->dval == 0) res = true;
	freeReplyObject(reply);
	return res;
}

/* get the latest ID of a connection from Redis */
uint64_t
redis_get_latest_rec(char *conn_name)
{
	redisReply *reply;
	uint64_t res;

	char *query = (char *)malloc(strlen("get ") + strlen(conn_name) + 1);
	strcpy(query, "get ");
	strcat(query, conn_name);

	reply = redisCommand(RCONTEXT, query);

	if (reply->str == NULL) /* key not found */ {
		res = 0;
	} else {
		char c;
		int scanned = sscanf(reply->str, "%"PRIu64 "%c", &res, &c);
		freeReplyObject(reply);
		if (scanned >= 1) return res;
		else return 0;
	}
	return 0;
}

/* Compare two entry structs */
bool
compare_entry(const Entry *lhs, const Entry *rhs)
{
	/* since each connection has its own Entry, it's enough to compare either the
	 * pointers or the file names for equality
	 */
	if (lhs->mem == rhs->mem) return true;
	return false;
}

/* Compare two gateway entry structs */
bool
compare_gentry(const GEntry *lhs, const GEntry *rhs)
{
	/* it's enough to check that the Entry members are the same */
	return compare_entry(&lhs->entry, &rhs->entry);
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

void
handle_kill_signal(int signal)
{
	keep_running = false;
}

/* main work loop
 * receive packets
 * create records
 * and update redis
 */
void *
run_loop(void *arg)
{
	printf("run loop\n");
	while (keep_running) {}
	printf("breaking out of the run loop\n");
	return NULL;
}

int
main(void)
{
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

	/* create a record for testing */
	int16_t rx_win = 10;
	uint32_t seq_num = 11;
	uint32_t ack_num = 12;
	u_int32_t dst_ip = 13;
	uint32_t src_ip = 14;
	uint16_t src_port = 5;
	uint16_t dst_port = 6;

	add_record(rx_win, seq_num, ack_num, dst_ip, src_ip, dst_port, src_port);

	/* every connection has the format `conn_hash_val` */
	char conn_name[] = "conn1";
	GEntry test_conn = create_gw_region(conn_name);
	if (compare_gentry(&test_conn, &GDEF)) return EXIT_FAILURE;
	printf("successfully created Entry struct for conn\n");

	/* redis test */
	uint64_t v = 9845;
	if (redis_add_latest_rec(conn_name, v)) printf("added to redis\n");
	uint64_t res = redis_get_latest_rec(conn_name);
	if (res == 0) printf("no data found\n");
	else printf("result: %"PRIu64"\n", res);

	/* running the main loop */
	pthread_attr_t attr;
	int s = pthread_attr_init(&attr);
	if (s != 0) perror("thread attributed not initialised");

	pthread_t thrd;
	if (pthread_create(&thrd, &attr, &run_loop, NULL) != 0) perror("thread creation");
	
	if (pthread_attr_destroy(&attr) != 0) perror("attribute destroy");
	if (pthread_join(thrd, NULL) != 0) perror("thread join");

	printf("main loop ended\n");


	/* after all is over */
	redisReply *reply;
	reply = redisCommand(RCONTEXT, "flushall");
	printf("clearing redis out: %s\n", reply->str);
	freeReplyObject(reply);
	redisFree(RCONTEXT);
	/* remove the pid file */
	if (remove(PID_FILE) == -1) perror("failed to remove pid file");
	if (fclose(NF_TAG_STRM) != 0) perror("failed to close nf tag stream");
	if (remove(NF_TAG_FILE) == -1) perror("failed to remove nf tag file");

	return EXIT_SUCCESS;
}