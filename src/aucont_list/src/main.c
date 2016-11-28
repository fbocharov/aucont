#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <container.h>

int main(int argc, char * argv[])
{
	container_t * containers;
	size_t count = 0;
	size_t i;

	count = containers_get_all(&containers);
	if (count < 0) {
		perror("cont get all:");
		return EXIT_FAILURE;
	}

	for (i = 0; i < count; ++i)
		fprintf(stdout, "%d\n", containers[i].pid);

	return EXIT_SUCCESS;
}
