#ifndef CGROUPS_H
#define CGROUPS_H

#include <stdbool.h>

int create_cpu_cgroup(int cont_pid, int cpu_perc);

int add_to_cpu_cgroup(int cont_pid, int pid);

#endif	// CGROUPS_H
