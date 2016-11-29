#include "cgroups.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#define CPU_CGROUP_PATH "/sys/fs/cgroup/cpu"
#define CONT_CPU_CGROUP_PATH_PATTERN CPU_CGROUP_PATH "/cont_%d_restriction"
#define CONT_CPU_CGROUP_PERIOD_FILE_PATH_PATTERN CONT_CPU_CGROUP_PATH_PATTERN "/cpu.cfs_period_us"
#define CONT_CPU_CGROUP_QUOTA_FILE_PATH_PATTERN CONT_CPU_CGROUP_PATH_PATTERN "/cpu.cfs_quota_us"
#define CONT_CPU_CGROUP_TASKS_FILE_PATH_PATTERN CONT_CPU_CGROUP_PATH_PATTERN "/tasks"

#define CPU_CGROUP_MAX_PERIOD 1000000

static int mount_cpu_cgroup()
{
	char cmd[200];

	if (!file_exists(CPU_CGROUP_PATH)) {
		sprintf(cmd, "sudo mkdir -p %s", CPU_CGROUP_PATH);
		if (system(cmd))
			return -1;

		sprintf(cmd, "sudo mount -t cgroup -ocpu %s", CPU_CGROUP_PATH);
		if (system(cmd))
			return -1;
	}

	return 0;
}

int create_cpu_cgroup(int cont_pid, int cpu_perc)
{
	char path[100];
	char cmd[200];
	size_t cpu_num;

	if (mount_cpu_cgroup())
		return -1;

	sprintf(path, CONT_CPU_CGROUP_PATH_PATTERN, cont_pid);
	sprintf(cmd, "sudo mkdir -p %s", path);
	if (system(cmd))
		return -1;

	cpu_num = sysconf(_SC_NPROCESSORS_ONLN);

	sprintf(path, CONT_CPU_CGROUP_PERIOD_FILE_PATH_PATTERN, cont_pid);
	sprintf(cmd, "echo %d | sudo tee --append %s > /dev/null", CPU_CGROUP_MAX_PERIOD, path);
	if (system(cmd) < 0)
		return -1;

	sprintf(path, CONT_CPU_CGROUP_QUOTA_FILE_PATH_PATTERN, cont_pid);
	sprintf(cmd, "echo %ld | sudo tee --append %s > /dev/null", CPU_CGROUP_MAX_PERIOD * cpu_num * cpu_perc / 100, path);
	if (system(cmd) < 0)
		return -1;


	return 0;
}

int add_to_cpu_cgroup(int cont_pid, int pid)
{
	char path[100];
	char cmd[200];

	sprintf(path, CONT_CPU_CGROUP_TASKS_FILE_PATH_PATTERN, cont_pid);
	sprintf(cmd, "echo %d | sudo tee --append %s > /dev/null", pid, path);
	if (system(cmd))
		return -1;

	return 0;
}
