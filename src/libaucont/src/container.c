#define _GNU_SOURCE
#include <sched.h>

#include "container.h"
#include "namespaces.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>


#define CONT_STACK_SIZE 2 * 1024 * 1024
char STACK[CONT_STACK_SIZE];


int container_exec_attr_init(container_exec_attr_t * attr,
	int argc, char * argv[])
{
	attr->argc = argc;
	attr->argv = (char **) malloc((argc + 1) * sizeof(*argv)); // +1 for NULL terminator
	if (!attr->argv)
		return -1;

	int i;
	for (i = 0; i < attr->argc; ++i) {
		attr->argv[i] = strdup(argv[i]);
		if (!attr->argv[i])
			goto err_cleanup;
	}
	attr->argv[attr->argc] = NULL;

	return 0;

err_cleanup:
	for (int j = 0; j < i; ++j)
		free(attr->argv[j]);
	free(attr->argv);
	return -1;
}

int container_exec_attr_destroy(container_exec_attr_t * attr)
{
	for (int i = 0; i < attr->argc; ++i)
		free(attr->argv[i]);
	free(attr->argv);
	return 0;
}


int container_net_attr_init(container_net_attr_t * attr,
	char const * host_ip, char const * cont_ip)
{
	attr->host_ip = strdup(host_ip);
	if (!attr->host_ip)
		return -1;

	attr->cont_ip = strdup(cont_ip);
	if (!attr->cont_ip) {
		free(attr->host_ip);
		return -1;
	}

	return 0;
}

int container_net_attr_destroy(container_net_attr_t * attr)
{
	free(attr->host_ip);
	free(attr->cont_ip);
	return 0;
}


int container_attr_init(container_attr_t * attr,
	char const * rootfs, container_exec_attr_t * exec)
{
	attr->rootfs = strdup(rootfs);
	if (!attr->rootfs)
		return -1;

	attr->daemonize = false;

	if (container_exec_attr_init(&attr->exec, exec->argc, exec->argv)) {
		free(attr->rootfs);
		return -1;
	}

	attr->net.host_ip = NULL;
	attr->net.cont_ip = NULL;

	attr->cpu_percentage = 100;

	return 0;
}

int container_attr_destroy(container_attr_t * attr)
{
	int ret = 0;
	int res = 0;
	free(attr->rootfs);
	ret = container_exec_attr_destroy(&attr->exec);
	if (ret)
		res = ret;

	ret = container_net_attr_destroy(&attr->net);
	if (ret)
		res = ret;

	return res;
}

int container_attr_setcpu(container_attr_t * attr, size_t percentage)
{
	attr->cpu_percentage = percentage;
	return 0;
}

int container_attr_setdaemon(container_attr_t * attr, bool daemon)
{
	attr->daemonize = daemon;
	return 0;
}

int container_attr_setnet(container_attr_t * attr, container_net_attr_t * net)
{
	if (container_net_attr_destroy(&attr->net))
		return -1;

	return container_net_attr_init(&attr->net, net->host_ip, net->cont_ip);
}


/*
 *	Code taken from http://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux
 */
static int daemonize()
{
	pid_t pid;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* Success: Let the parent terminate */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* On success: The child process becomes session leader */
	if (setsid() < 0)
		return -1;

	// Fork off for the second time
	pid = fork();

	/* An error occurred */
	if (pid < 0)
		return -1;

	/* Success: Let the parent terminate */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	// if (chdir("/"))
	// 	return -1;

	/* Close all open file descriptors */
	for (int fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
		if (fd < 3)
			close(fd);
	}

	/* Set new file permissions */
	umask(027);

	return 0;
}

typedef struct {
	container_attr_t * attrs;
	int pipe_from_host;
	int pipe_to_host;
} cont_start_params_t;

static int start_container(void * arg)
{
	cont_start_params_t * params = (cont_start_params_t *) arg;

	if (params->attrs->daemonize && daemonize())
		return -1;

	if (unshare(CLONE_NEWPID)) {
		perror("unshare");
		return EXIT_FAILURE;
	}

	int cont_fds[2];
	if (pipe2(cont_fds, O_CLOEXEC)) {
		perror("pipe2");
		return EXIT_FAILURE;
	}

	int pid = fork();
	if (pid < 0) {
		perror("fork");
		return EXIT_FAILURE;
	}

	if (pid == 0) {
		if (read(cont_fds[0], &pid, sizeof(pid)) < 0) {
			perror("cont child: read pid");
			return EXIT_FAILURE;
		}

		if (write(params->pipe_to_host, &pid, sizeof(pid)) < 0) {
			perror("cont child: write pid to host");
			return EXIT_FAILURE;
		}

		// wait for user namespace to be configured
		int msg;
		if (read(params->pipe_from_host, &msg, sizeof(msg)) < 0) {
			perror("cont child: read-wait for user ns");
			return EXIT_FAILURE;
		}

		if (setup_uts_ns("container")) {
			perror("cont child: setup_uts_ns");
			return EXIT_FAILURE;
		}

		if (setup_mount_ns(params->attrs->rootfs)) {
			perror("cont child: setup_mount_ns");
			return EXIT_FAILURE;
		}

		if (execv(params->attrs->exec.argv[0], params->attrs->exec.argv)) {
			fprintf(stderr, "failed to exec");
			perror("cont child: execvp");
			return EXIT_FAILURE;
		}
	}

	// now child will do all ipc with host to prevent race on host<->cont pipe
	if (write(cont_fds[1], &pid, sizeof(pid)) < 0) {
		perror("cont: write");
		return EXIT_FAILURE;
	}

	close(cont_fds[0]);
	close(cont_fds[1]);

	if (wait(NULL) < 0) {
		perror("cont: waitpid");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int container_create(container_t * container, container_attr_t * attr)
{
	int ret = 0;

	container->rootfs = strdup(attr->rootfs);
	if (!container->rootfs)
		return -1;

	container->daemon = attr->daemonize;

	int flags = CLONE_NEWIPC  | CLONE_NEWNET | CLONE_NEWNS | 
				CLONE_NEWUSER | CLONE_NEWUTS;
	if (!attr->daemonize)
		flags |= SIGCHLD;

	cont_start_params_t params;
	params.attrs = attr;

	int to_cont[2];
	if (pipe2(to_cont, O_CLOEXEC)) {
		perror("pipe");
		ret = -1;
		goto cleanup;
	}
	params.pipe_from_host = to_cont[0];
	int pipe_to_cont = to_cont[1];

	int from_cont[2];
	if (pipe2(from_cont, O_CLOEXEC)) {
		perror("pipe");
		ret = -1;
		goto cleanup;
	}
	params.pipe_to_host = from_cont[1];
	int pipe_from_cont = from_cont[0];

	if (clone(start_container, STACK + CONT_STACK_SIZE, flags, &params) < 0)
		goto cleanup;

	int pid;
	if (read(pipe_from_cont, &pid, sizeof(pid)) < 0) {
		perror("read pid");
		ret = -1;
		goto cleanup;
	}
	container->pid = pid;

	if (setup_user_ns(pid)) {
		perror("setup user ns");
		ret = -1;
		goto cleanup;
	}
	
	int msg = 1;
	if (write(pipe_to_cont, &msg, sizeof(msg)) < 0) {
		perror("cont: write after setup user ns");
		ret = -1;
		goto cleanup;
	}

	close(to_cont[0]);
	close(to_cont[1]);
	close(from_cont[0]);
	close(from_cont[1]);

cleanup:
	free(container->rootfs);
	return ret;
}

int container_wait(container_t * container)
{
	if (wait(NULL) >= 0 || errno == ECHILD)
		return 0;
	return -1;
}

int container_stop(container_t * container, int signum)
{
	fprintf(stderr, "killing %d\n", container->pid);

	if (kill(container->pid, signum) && errno != ESRCH)
		return -1;

	return 0;
}

int container_exec(container_t * container, container_exec_attr_t * attr)
{
	if (enter_ns(container->pid, "user"))
		return -1;

	if (enter_ns(container->pid, "ipc"))
		return -1;

	if (enter_ns(container->pid, "net"))
		return -1;

	if (enter_ns(container->pid, "uts"))
		return -1;

	if (enter_ns(container->pid, "pid"))
		return -1;

	if (enter_ns(container->pid, "mnt"))
		return -1;

	int pd = fork();
	if (pd < 0) {
		perror("fork");
		return -1;
	}

	if (pd == 0) {
		if (execv(attr->argv[0], attr->argv)) {
			perror("execvp");
			return EXIT_FAILURE;
		}
	}

	if (wait(NULL) < 0) {
		perror("wait");
		return -1;
	}

	return 0;
}



#define MAX_CONT_COUNT 1024
container_t containers[MAX_CONT_COUNT];
const char * containers_list_filename = "containers.list";

static int load_containters()
{
	if (!file_exists(containers_list_filename)) {
		return 0;
	}

	int cont_file = open(containers_list_filename, O_RDONLY);
	if (cont_file < 0) {
		perror("open");
		return -1;
	}

	int pid;
	int i = 0;
	while (read(cont_file, &pid, sizeof(pid)) > 0) {
		containers[i].pid = pid;
		++i;
	}

	return i;
}

static int save_containers(int count)
{
	int cont_file = creat(containers_list_filename, S_IRUSR | S_IWUSR);
	if (cont_file < 0) {
		perror("open");
		return -1;
	}

	for (int i = 0; i < count; ++i) {
		int pid = containers[i].pid;
		if (write(cont_file, &pid, sizeof(pid)) < 0) {
			perror("write");
			return -1;
		}
	}

	if (close(cont_file)) {
		perror("close");
		return -1;
	}

	return 0;
}

int containers_add(container_t * cont)
{
	int count = load_containters();
	// XXX: rootfs saved incorrectly, avoid using it!!!
	containers[count] = *cont;
	save_containers(count + 1);
	return 0;
}

int containers_rm(container_t * cont)
{
	int count = load_containters();
	if (count == 0)
		return 0;

	int i;
	for (i = 0; i < count; ++i) {
		if (containers[i].pid == cont->pid)
			break;
	}

	for (; i + 1 < count; ++i)
		containers[i] = containers[i + 1];

	save_containers(count - 1);

	return 0;
}


