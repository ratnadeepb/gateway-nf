#include "common.h"
#include "hiredis.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>

static redisContext *RCONTEXT; /* a global connection to the Redis DB */

bool redis_add(char *conn_name, uint64_t id)
{
	bool res = false;

	redisReply *reply;

	char *val = (char *)malloc(sizeof(uint64_t) + 1);
	if (val == NULL) {
		perror("failed creating val\n");
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

uint64_t redis_get_record_id(char *conn_name)
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

bool compare_entry(const Entry *lhs, const Entry *rhs)
{
	/* since each connection has its own Entry, it's enough to compare either the
	 * pointers or the file names for equality
	 */
	// if (strncmp(lhs->file_name, rhs->file_name, sizeof(lhs->file_name)) != 0) return false;
	if (lhs->mem == rhs->mem) return true;
	return false;
}

bool compare_gentry(const GEntry *lhs, const GEntry *rhs)
{
	/* it's enough to check that the Entry members are the same */
	return compare_entry(&lhs->entry, &rhs->entry);
}

GEntry create_gw_region(char *conn_name)
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
		perror("truncate failure\n");
		return GDEF;
	}

	struct stat sb;
	Record *mem;
	if (fstat(fd, &sb) == 0) {
		mem = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mem == MAP_FAILED) {
			perror("mapping failed\n");
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

int main()
{
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

	/* after all is over */
	redisFree(RCONTEXT);

	return EXIT_SUCCESS;
}