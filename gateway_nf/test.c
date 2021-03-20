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

static redisContext *RCONTEXT; /* a global connection to the Redis DB */
static FILE *NF_TAG_STRM;
static bool keep_running = true;

static word_t GLOBAL_BITMAP = 0;

static char NF_LIST[sizeof(uint64_t)][MAX_NF_TAG_SZ];

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

bool
redis_add(char *conn_name, uint64_t id)
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

	reply = redisCommand(RCONTEXT, cmd);
	if ((int)reply->dval == 0) res = true;
	freeReplyObject(reply);
	return res;
}

uint64_t
redis_get_record_id(char *conn_name)
{
	redisReply *reply;
	uint64_t res;

	char *query = (char *)malloc(strlen("get ") + strlen(conn_name) + 1);
	strcpy(query, "get ");
	strcat(query, conn_name);

	reply = redisCommand(RCONTEXT, query);

	char c;
	int scanned = sscanf(reply->str, "%"PRIu64 "%c", &res, &c);
	freeReplyObject(reply);
	if (scanned >= 1) return res;
	else return 0;
}

bool
compare_entry(const Entry *lhs, const Entry *rhs)
{
	/* since each connection has its own Entry, it's enough to compare either the
	 * pointers or the file names for equality
	 */
	// if (strncmp(lhs->file_name, rhs->file_name, sizeof(lhs->file_name)) != 0) return false;
	if (lhs->mem == rhs->mem) return true;
	return false;
}

bool
compare_gentry(const GEntry *lhs, const GEntry *rhs)
{
	/* it's enough to check that the Entry members are the same */
	return compare_entry(&lhs->entry, &rhs->entry);
}

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

void
create_nf_tag_list_file()
{
	creat(NF_TAG_FILE, O_CREAT | S_IRUSR | S_IWRITE | S_IWGRP | S_IRGRP | S_IROTH | S_IWOTH);
	/* gateway only reads from this file */
	NF_TAG_STRM = fopen(NF_TAG_FILE, "r");
}

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

	signal(SIGUSR1, handle_user_signals);
	signal(SIGKILL, handle_kill_signal);
	signal(SIGQUIT, handle_kill_signal);
	signal(SIGINT, handle_kill_signal);

	char conn_name[] = "conn1";
	GEntry test_conn = create_gw_region(conn_name);
	if (compare_gentry(&test_conn, &GDEF)) return EXIT_FAILURE;
	printf("successfully created Entry struct for conn\n");

	/* redis test */
	RCONTEXT = redisConnect("localhost", 6379);
	if (RCONTEXT != NULL && RCONTEXT->err) printf("Error %s\n", RCONTEXT->errstr);
	else printf("connected to redis\n");

	uint64_t v = 9845;
	if (redis_add(conn_name, v)) printf("added to redis\n");
	uint64_t res = redis_get_record_id(conn_name);
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

	printf("out of loop\n");


	/* after all is over */
	// redisFree(RCONTEXT);
	/* remove the pid file */
	if (remove(PID_FILE) == -1) perror("failed to remove pid file");
	if (fclose(NF_TAG_STRM) != 0) perror("failed to close nf tag stream");
	if (remove(NF_TAG_FILE) == -1) perror("failed to remove nf tag file");

	return EXIT_SUCCESS;
}