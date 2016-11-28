#ifndef CONTAINER_H
#define CONTAINER_H


#include <sys/types.h>
#include <stdbool.h>


typedef struct {
	/* should be at least 1 */
	int argc;

	/* argv[0] treated as command to run */
	char ** argv;
} container_exec_attr_t;

int container_exec_attr_init(container_exec_attr_t * attr,
	int argc, char * argv[]);
int container_exec_attr_destroy(container_exec_attr_t * attr);


typedef struct {
	char * host_ip;
	char * cont_ip;
} container_net_attr_t;

int container_net_attr_init(container_net_attr_t * attr,
	char const * host_ip, char const * cont_ip);
int container_net_attr_destroy(container_net_attr_t * attr);


typedef struct {
	char * rootfs;

	container_exec_attr_t exec;
	bool daemonize;

	container_net_attr_t net;

	size_t cpu_percentage;
} container_attr_t;


int container_attr_init(container_attr_t * attr,
	char const * rootfs, container_exec_attr_t * exec);
int container_attr_destroy(container_attr_t * attr);

int container_attr_setcpu(container_attr_t * attr, size_t percentage);
int container_attr_setdaemon(container_attr_t * attr, bool daemon);
int container_attr_setnet(container_attr_t * attr, container_net_attr_t * net);


typedef struct {
	pid_t pid;
	bool daemon;
	char * rootfs;
	// ip?
} container_t;


int container_create(container_t * container, container_attr_t * attr);

int container_wait(container_t * container);

int container_stop(container_t * container, int signum);

int container_exec(container_t * container, container_exec_attr_t * attr);


int containers_add(container_t * cont);

int containers_rm(container_t * cont);

int containers_get(int pid, container_t * cont);

int containers_get_all(container_t ** conts);



#endif	// CONTAINER_H
