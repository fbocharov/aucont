#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <container.h>

void usage(char const * name)
{
	printf("usage: %s PID [SIG_NUM]\n", name);
	printf("Kills root container process with signal and cleans up container resources\n");
	printf("  PID - container init process pid in its parent PID namespace\n");
	printf("  SIG_NUM - number of signal to send to container process;\n");
	printf("            default is SIGTERM (15)\n");
}

int main(int argc, char * argv[])
{
	int pid;
	int signum;
	container_t cont;
	int err = 0;

	if (argc != 2 && argc != 3) {
		printf("%d\n", argc);
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	pid = atoi(argv[1]);
	signum = SIGTERM;
	if (argc == 3)
		signum = atoi(argv[2]);

	err = containers_get(pid, &cont);
	if (err < 0) {
		perror("cont get:");
		return EXIT_FAILURE;
	} else if (err == 0) {
		// no container with such pid
		return EXIT_SUCCESS;
	}

	if (container_stop(&cont, signum)) {
		perror("container_stop");
		return EXIT_FAILURE;
	}

	containers_rm(&cont);

	return EXIT_SUCCESS;
}
