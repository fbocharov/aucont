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


char const * containers_filename = "containers.list";

int main(int argc, char * argv[])
{
	if (argc != 2 && argc != 3) {
		printf("%d\n", argc);
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	int pid = atoi(argv[1]);
	int signum = SIGTERM;
	if (argc == 3)
		signum = atoi(argv[2]);

	// dirty but fast, redo in next version
	container_t cont;
	cont.pid = pid;

	if (container_stop(&cont, signum)) {
		perror("container_stop");
		return EXIT_FAILURE;
	}

	containers_rm(&cont);

	return EXIT_SUCCESS;
}
