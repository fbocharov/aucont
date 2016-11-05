#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <container.h>

void usage(char const * name)
{
	printf("usage: %s PID CMD [ARGS]\n", name);
	printf("  PID - container init process pid in its parent PID namespace\n");
	printf("  CMD - command to run inside container\n");
	printf("  ARGS - arguments for CMD\n");
}

int main(int argc, char * argv[])
{
	if (argc < 3) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	container_t cont;
	cont.pid = atoi(argv[1]);

	container_exec_attr_t attr;
	if (container_exec_attr_init(&attr, argc - 2, argv + 2)) {
		perror("cont_exec_attr");
		return EXIT_FAILURE;
	}

	if (container_exec(&cont, &attr)) {
		perror("cont_exec");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
