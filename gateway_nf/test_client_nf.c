#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

pid_t
get_gw_pid()
{
	int pid_f = open(PID_FILE, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (pid_f == -1) {
		perror("error opening pid file");
		return -1;
	}
	char *buf = (char *)malloc(sizeof(int));
	read(pid_f, buf, 10);

	errno = 0;
	char *endptr;
	pid_t pid = (pid_t)strtol(buf, &endptr, 10);
	if ((errno == ERANGE && (pid == LONG_MAX || pid == LONG_MIN)) || (errno != 0 && pid == 0)) {
		perror("strtol");
		return -1;
	}

	return pid;
}

void
write_to_nf_tag_list_file(char *nf_tag)
{
	/* nf only writes to this file */
	int nf_tag_f = open(NF_TAG_FILE, O_WRONLY, S_IWUSR);
	if (nf_tag_f == -1) {
		perror("error opening nf tag file");
		return;
	}

	/* seek to the end */
	lseek(nf_tag_f, 0, SEEK_END);

	char *buf = (char *)malloc(strlen(nf_tag) + 2);
	strcpy(buf, nf_tag);
	strcat(buf, "\n");
	int len = write(nf_tag_f, buf, strlen(buf));
	if (len == -1) perror("writing error");
}

void
send_registration(char *nf_tag)
{
	pid_t pid = get_gw_pid();
	printf("gw pid: %d\n", pid);

	/* signal gateway there is a new NF */
	write_to_nf_tag_list_file(nf_tag);
	if (kill(pid, SIGUSR1) == -1) perror("sending signal failed");
}

int
main(int argc, char **argv)
{
	char *nf_tag;
	if (argc > 1) nf_tag = argv[1];
	else nf_tag = "nf_tag";
	
	send_registration(nf_tag);

	return 0;
}