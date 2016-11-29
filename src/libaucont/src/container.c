#define _GNU_SOURCE
#include <sched.h>

#include <assert.h>
#include <stdlib.h>

#include "container.h"
#include "namespaces.h"
#include "cgroups.h"
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
	int i;

	attr->argc = argc;
	attr->argv = (char **) malloc((argc + 1) * sizeof(*argv)); // +1 for NULL terminator
	if (!attr->argv)
		return -1;

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
	int i;
	for (i = 0; i < attr->argc; ++i)
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
	if (percentage > 100) {
		errno = EINVAL;
		return -1;
	}

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
	int fd;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if (pid < 0)
		return -1;

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
	if (chdir("/"))
		return -1;

	fd = open("/dev/null", O_RDWR, 0);
	if (fd < 0) {
		perror("open /dev/null");
		return -1;
	}

	if (dup2(fd, STDIN_FILENO) < 0) {
		perror("dup2 stdin");
		return -1;
	}

	if (dup2(fd, STDOUT_FILENO) < 0) {
		perror("dup2 stdout");
		return -1;
	}

	if (dup2(fd, STDERR_FILENO) < 0) {
		perror("dup2 stderr");
		return -1;
	}

	close(fd);

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

	if (params->attrs->daemonize && daemonize()) {
		perror("container starter failed to daemonize");
		return EXIT_FAILURE;
	}

	if (unshare(CLONE_NEWPID)) {
		perror("container starter failed to unshare");
		return EXIT_FAILURE;
	}

	int cont_fds[2];
	if (pipe2(cont_fds, O_CLOEXEC)) {
		perror("container starter failed to create pipes");
		return EXIT_FAILURE;
	}

	int pid = fork();
	if (pid < 0) {
		perror("container starter failed to fork");
		return EXIT_FAILURE;
	}

	if (pid == 0) {
		if (read(cont_fds[0], &pid, sizeof(pid)) < 0) {
			perror("container command executor failed to receive pid");
			return EXIT_FAILURE;
		}

		if (write(params->pipe_to_host, &pid, sizeof(pid)) < 0) {
			perror("container command executor failed to send pid");
			return EXIT_FAILURE;
		}

		// wait for user namespace and cgroups (maybe) to be configured
		int msg;
		if (read(params->pipe_from_host, &msg, sizeof(msg)) < 0) {
			perror("container command executor failed to receive sync message");
			return EXIT_FAILURE;
		}

		if (setup_uts_ns("container")) {
			perror("container command executor failed to setup hostname");
			return EXIT_FAILURE;
		}

		if (setup_mount_ns(params->attrs->rootfs)) {
			perror("container command executor failed to mount rootfs");
			return EXIT_FAILURE;
		}

		if (execv(params->attrs->exec.argv[0], params->attrs->exec.argv)) {
			perror("container command executor failed to exec");
			return EXIT_FAILURE;
		}
	}

	// now child will do all ipc with host to prevent race on host<->cont pipe
	if (write(cont_fds[1], &pid, sizeof(pid)) < 0) {
		perror("container starter failed to send pid");
		return EXIT_FAILURE;
	}

	close(cont_fds[0]);
	close(cont_fds[1]);

	if (wait(NULL) < 0) {
		perror("container starter failed to wait child");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int container_create(container_t * container, container_attr_t * attr)
{
	int err = 0;
	int flags = 0;
	int to_cont[2];
	int from_cont[2];
	int pipe_to_cont, pipe_from_cont;
	int pid;
	int msg;

	container->rootfs = strdup(attr->rootfs);
	if (!container->rootfs)
		return -1;

	container->daemon = attr->daemonize;

	flags = CLONE_NEWIPC  | CLONE_NEWNET | CLONE_NEWNS |
				CLONE_NEWUSER | CLONE_NEWUTS;
	if (!attr->daemonize)
		flags |= SIGCHLD;

	cont_start_params_t params;
	params.attrs = attr;

	err = pipe2(to_cont, O_CLOEXEC);
	if (err)
		goto cleanup;
	params.pipe_from_host = to_cont[0];
	pipe_to_cont = to_cont[1];

	err = pipe2(from_cont, O_CLOEXEC);
	if (err)
		goto cleanup;
	params.pipe_to_host = from_cont[1];
	pipe_from_cont = from_cont[0];

	err = clone(start_container, STACK + CONT_STACK_SIZE, flags, &params);
	if (err < 0)
		goto cleanup;

	err = read(pipe_from_cont, &pid, sizeof(pid));
	if (err < 0)
		goto cleanup;
	container->pid = pid;

	err = setup_user_ns(pid);
	if (err)
		goto cleanup;

	err = create_cpu_cgroup(pid, attr->cpu_percentage);
	if (err)
		goto cleanup;

	err = add_to_cpu_cgroup(pid, pid);
	if (err)
		goto cleanup;
	
	msg = 1;
	err = write(pipe_to_cont, &msg, sizeof(msg));
	if (err < 0)
		goto cleanup;

	close(to_cont[0]);
	close(to_cont[1]);
	close(from_cont[0]);
	close(from_cont[1]);

	return 0;

cleanup:
	free(container->rootfs);
	return err;
}

int container_wait(container_t * container)
{
	if (wait(NULL) >= 0 || errno == ECHILD)
		return 0;
	return -1;
}

int container_stop(container_t * container, int signum)
{
	if (kill(container->pid, signum) && errno != ESRCH)
		return -1;
	return 0;
}

int container_exec(container_t * container, container_exec_attr_t * attr)
{
	int pid;
	int p[2];
	int msg;

	if (pipe2(p, O_CLOEXEC))
		return -1;

	if (add_to_cpu_cgroup(container->pid, getpid())) {
		perror("host executor failed to setup cgroup");
		return -1;
	}

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

	pid = fork();
	if (pid < 0)
		return -1;

	if (pid == 0) {
		if (read(p[0], &msg, sizeof(msg)) < 0) {
			perror("container executor failed to receive sync msg");
			return EXIT_FAILURE;
		}

		// running command in container
		if (execv(attr->argv[0], attr->argv)) {
			perror("container executor failed to exec");
			return EXIT_FAILURE;
		}
	}

	msg = 1;
	if (write(p[1], &msg, sizeof(msg)) < 0) {
		perror("host executor failed to send sync msg");
		return -1;
	}

	if (wait(NULL) < 0)
		return -1;

	return 0;
}



#define CONT_LIST_FILENAME "containers.list"
#define MAX_CONT_COUNT 1024
container_t containers[MAX_CONT_COUNT];

static int load_containters(size_t * count)
{
	int pid;
	int i = 0;
	if (!file_exists(CONT_LIST_FILENAME))
		return 0;

	int cont_file = open(CONT_LIST_FILENAME, O_RDONLY);
	if (cont_file < 0)
		return -1;

	while (read(cont_file, &pid, sizeof(pid)) > 0) {
		containers[i].pid = pid;
		++(*count);
		++i;
	}

	return 0;
}

static int save_containers(size_t count)
{
	int cont_file;
	int i;

	assert(count <= MAX_CONT_COUNT);

	cont_file = creat(CONT_LIST_FILENAME, S_IRUSR | S_IWUSR);
	if (cont_file < 0)
		return -1;

	for (i = 0; i < count; ++i) {
		int pid = containers[i].pid;
		if (write(cont_file, &pid, sizeof(pid)) < 0)
			return -1;
	}

	if (close(cont_file))
		return -1;

	return 0;
}

int containers_add(container_t * cont)
{
	size_t count = 0;

	if (load_containters(&count))
		return -1;

	containers[count] = *cont;

	return save_containers(count + 1);
}

int containers_rm(container_t * cont)
{
	size_t i;
	size_t count = 0;
	if (load_containters(&count))
		return -1;

	if (count == 0)
		return 0;

	for (i = 0; i < count; ++i) {
		if (containers[i].pid == cont->pid)
			break;
	}

	for (; i + 1 < count; ++i)
		containers[i] = containers[i + 1];

	save_containers(count - 1);

	return 0;
}

int containers_get(int pid, container_t * cont)
{
	size_t i;
	size_t count = 0;
	if (load_containters(&count))
		return -1;

	for (i = 0; i < count; ++i) {
		if (containers[i].pid == pid) {
			*cont = containers[i];
			return 1;
		}
	}

	return 0;
}

int containers_get_all(container_t ** conts)
{
	size_t i = 0;
	size_t cnt = 0;
	if (load_containters(&cnt))
		return -1;

	*conts = (container_t *) calloc(cnt, sizeof(**conts));
	if (!*conts)
		return -1;

	for (i = 0; i < cnt; ++i)
		(*conts)[i].pid = containers[i].pid;

	return cnt;
}
