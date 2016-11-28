#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <container.h>

bool daemonize = false;
int cpu_perc = 100;
char * cont_ip = NULL;


char * img_path = NULL;
int args_cnt = 0;
char ** args = NULL;


void usage(char const * name) 
{
	printf("usage: %s [-d --cpu CPU_PERC --net IP] IMAGE_PATH CMD [ARGS]\n", name);
	printf("  IMAGE_PATH - path to image of container file system\n");
	printf("  CMD - command to run inside container\n");
	printf("  ARGS - arguments for CMD\n");
	printf("  -d - daemonize\n");
	printf("  --cpu CPU_PERC - percent of cpu resources allocated for container 0..100\n");
	printf("  --net IP - create virtual network between host and container;\n");
	printf("        IP - container ip address, IP+1 - host side ip address\n");
}

int parse_args(int argc, char * argv[])
{
	int last_opt_arg = argc - 2;

	int i = 1;
	for ( ; i < last_opt_arg; ++i) {
		if (!strcmp("-d", argv[i])) {
			daemonize = true;
		} else if (!strcmp("--cpu", argv[i])) {
			if (i + 1 >= last_opt_arg || argv[i + 1][0] == '-')
				return -1;

			cpu_perc = atoi(argv[i + 1]);
			++i;
		} else if (!strcmp("--net", argv[i])) {
			if (i + 1 >= last_opt_arg || argv[i + 1][0] == '-')
				return -1;

			cont_ip = strdup(argv[i + 1]);
			if (!cont_ip) {
				perror("strdup");
				exit(1);
			}
			++i;
		} else {
			// no more optional arguments left -> can parse other arguments
			break;
		}
	}

	img_path = strdup(argv[i]);
	++i;

	args_cnt = argc - i;
	args = (char **) malloc((args_cnt + 1) * sizeof(*args)); // +1 for NULL terminator
	if (!args) {
		perror("malloc");
		exit(1);
	}

	int j;
	for (j = 0; j < args_cnt; ++j, ++i) {
		args[j] = strdup(argv[i]);
		if (!args[j]) {
			perror("strdup");
			exit(1);
		}
	}
	args[args_cnt] = NULL;

	return 0;
}


int setup_daemon(container_attr_t * cont_attr)
{
	if (daemonize && container_attr_setdaemon(cont_attr, daemonize)) {
		perror("cont_attr_setdaemon");
		return -1;
	}
	return 0;
}

int setup_net(container_attr_t * cont_attr)
{
	// TODO: implement
	return 0;
}

int setup_cpu(container_attr_t * cont_attr)
{
	if (cpu_perc != 100 && container_attr_setcpu(cont_attr, cpu_perc)) {
		perror("container_attr_setcpu");
		return EXIT_FAILURE;
	}
	return 0;
}


int main(int argc, char * argv[]) 
{
	if (argc < 3) {
		usage(argv[0]);
		return EXIT_SUCCESS;
	}

	if (parse_args(argc, argv)) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	container_exec_attr_t exec;
	if (container_exec_attr_init(&exec, args_cnt, args)) {
		perror("cont_exec_attr_init");
		return EXIT_FAILURE;
	}

	container_attr_t cont_attr;
	if (container_attr_init(&cont_attr, img_path, &exec)) {
		perror("cont_attr_init");
		return EXIT_FAILURE;
	}


	if (setup_cpu(&cont_attr) || setup_net(&cont_attr) || setup_daemon(&cont_attr))
		return EXIT_FAILURE;

	container_t cont;
	if (container_create(&cont, &cont_attr)) {
		perror("cont_create");
		return EXIT_FAILURE;
	}


	printf("%d\n", cont.pid);
	if (daemonize) {
		containers_add(&cont);
	} else {
		if (container_wait(&cont)) {
			perror("cont_wait");
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
