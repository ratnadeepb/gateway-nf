#include "init.h"


void create_regions()
{
	int p_fd = open(BASE_PROC_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	int c_fd = open(BASE_COMPL_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	if (p_fd == -1) perror("error creating processing area\n");
	if (c_fd == -1) perror("error creating completed area\n");

	if (ftruncate(p_fd, NUM_PAGES * NUM_SLOTS * sizeof(Record)) != 0) {
		perror("p truncate failure\n");
		return EXIT_FAILURE;
	}

	if (ftruncate(c_fd, NUM_PAGES * NUM_SLOTS * sizeof(Record)) != 0) {
		perror("c truncate failure\n");
		return EXIT_FAILURE;
	}

	struct stat p_sb;
	if (fstat(p_fd, &p_sb) == 0) {
		assert(p_sb.st_size != 0);
	}

	struct stat c_sb;
	if (fstat(c_fd, &p_sb) == 0) {
		assert(c_sb.st_size != 0);
	}

	Record *p_mem = mmap(NULL, p_sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, p_fd, 0);
	if (p_mem == MAP_FAILED) {
		perror("p mapping failed\n");
		return EXIT_FAILURE;
	}

	Record *c_mem = mmap(NULL, c_sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, c_fd, 0);
	if (c_mem == MAP_FAILED) {
		perror("c mapping failed\n");
		return EXIT_FAILURE;
	}

	PROCESSING = p_mem;
	COMPLETED = c_mem;

	return EXIT_SUCCESS;
}