#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


char const * containers_filename = "containers.list";


int main(int argc, char * argv[])
{
	int cont_file = open(containers_filename, O_RDONLY);
	if (!cont_file) {
		perror("fopen");
		return -1;
	}

	int pid;
	while (read(cont_file, &pid, sizeof(pid)) > 0)
		printf("%d\n", pid);

	close(cont_file);

	return 0;
}
