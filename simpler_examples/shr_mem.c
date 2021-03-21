/* for mmap */
#include <sys/mman.h>
/* for open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

/* for INT_MAX and INT_MAN */
#include <limits.h>


#include <stdlib.h>

/* for NULL and ftruncate */
#include <unistd.h>

#define NUM_PAGES 1024 /* each page corresponds to a connection */
#define NUM_SLOTS 24   /* each slot corresponds to a header */

typedef enum{false, true} bool;

// #ifndef bool
// #define bool bool
// #endif
// #ifndef true
// #define true true
// #endif
// #ifndef false
// #define false false
// #endif

typedef struct Conn {
	int16_t rx_win;
	uint32_t seq_num;
	uint32_t ack_num;
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t port;
} Conn;

typedef struct Record {
	int16_t id;
	Conn connection;
	bool drop_packet;
} Record;

/* create a new record */
Record new_record(Conn connection, int16_t id, bool drop_packet)
{
	Record rec = { id, connection, drop_packet };
	return rec;
}

/* 
 * alter dst_ip is an example of attempts at modifying triggers copy on write (CoW)
 * @rec -> original pointer that is to be altered
 * @mem -> head of the memory region to hold the records queue
 * @offset -> where in the queue the new record needs to be stored at
 */
void
alter_dst_ip(Record *rec, Record *mem, off_t offset)
{
	Record *ptr = mem + offset;
	memcpy(ptr, rec, sizeof(Record));
	ptr->connection.dst_ip += 1;
}

int
main(int argc, char **argv)
{
	long srv;
	if (argc > 0) {
		char *p;
		errno = 0;
		srv = strtol(argv[1], &p, 10);
		assert(errno == 0 || *p != '\0' || srv > INT_MAX || srv < INT_MIN);
	} else {
		return EXIT_FAILURE;
	}

	if (srv == 1) { /* this is the server */
		printf("This is the server. Hi to the server! We are going to write things\n");
		/* define a new conn */
		Conn conn1 = {
			.rx_win = 10,
			.seq_num = 11,
			.ack_num = 12,
			.dst_ip = 13,
			.src_ip = 14,
			.port = 5,
		};

		int16_t id = 1;
		bool drop_packet = false;

		Record rec = new_record(conn1, id, drop_packet);
		
		int fd = open("/dev/shm/tmpfile", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (ftruncate(fd, NUM_PAGES * NUM_SLOTS * sizeof(Record)) != 0) {
			printf("truncate failure\n");
			return EXIT_FAILURE;
		}

		struct stat sb;
		if (fstat(fd, &sb) == 0) {
			printf("Size of the file is: %ld\n", sb.st_size);
			Record *mem = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (mem == MAP_FAILED) {
				printf("mapping failed\n");
				return EXIT_FAILURE;
			}
			printf("mem is at: %p\n", mem);
			Record *tmp = mem + sizeof(Record) * 10;
			*tmp = rec;
			printf("rx_win: %" PRId16 "\n", tmp->connection.rx_win);
			printf("src_ip: %" PRId16 "\n", tmp->connection.src_ip);
			printf("dst_ip: %" PRId16 "\n", tmp->connection.dst_ip);
			assert(tmp->connection.rx_win == 10);
			assert(tmp->connection.src_ip == 14);
			printf("tmp is at: %p\n", tmp);
		}
	} else if (srv == 2) { /* this is the client */
		printf("This is the client. Hi to the client! We are going to read things\n");
		/* for now it's read only, but we would later want clients to 
		* be able to change certain parts of the records */
		int fd = open("/dev/shm/tmpfile", O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		assert(fd != -1);

		struct stat sb;
		Record *tmp;
		if (fstat(fd, &sb) == 0) {
			printf("Size of the file is: %ld\n", sb.st_size);
			Record *mem = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (mem == MAP_FAILED) {
				printf("mapping failed in client\n");
				return EXIT_FAILURE;
			}
			printf("mem is at: %p\n", mem);
			tmp = mem + sizeof(Record) * 10;
			printf("tmp is at: %p\n", tmp);
			printf("rx_win: %" PRId16 "\n", tmp->connection.rx_win);
			printf("src_ip: %" PRId16 "\n", tmp->connection.src_ip);
			printf("dst_ip: %" PRId16 "\n", tmp->connection.dst_ip);
			assert(tmp->connection.rx_win == 10);
			assert(tmp->connection.src_ip == 14);
		}

		int local_fd = open("/dev/shm/localfile", O_CREAT | O_RDWR, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		assert(fd != -1);
		if (ftruncate(local_fd, NUM_SLOTS * sizeof(Record)) != 0) {
			printf("truncate failure for local memory\n");
			return EXIT_FAILURE;
		}
		/* 
		 * this does not need to open an explicit file
		 * it can use `MAP_ANONYMOUS` as well
		 */
		Record *local = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (local == MAP_FAILED) {
			printf("mapping failed in local file mapping\n");
			return EXIT_FAILURE;
		}
		/* now we try to alter the dst_ip */
		alter_dst_ip(tmp, local, 5);
		Record *ltmp = local + 5;
		printf("local dst_ip: %" PRId16 "\n", ltmp->connection.dst_ip);
	} else {
		printf("This is neither the server nor the client\n");
		printf("What are we supposed to do with this!");
		return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}