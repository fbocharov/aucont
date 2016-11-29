#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include "namespaces.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>


int setup_mount_ns(char const * root)
{
	if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL))
		return -1;

	char path[100];
	sprintf(path, "%s/proc", root);
	if (mount("proc", path, "proc", MS_NOEXEC, NULL))
		return -1;

	sprintf(path, "%s/sys", root);
	if (mount("sysfs", path, "sysfs", MS_NOEXEC, NULL))
		return -1;

	char const * old_root = ".old_root_basta228";
	// make dir and make root bindable to allow pivot root
	sprintf(path, "%s/%s", root, old_root);
	if (!file_exists(path) && mkdir(path, 0777))
		return -1;

	if (mount(root, root, "bind", MS_BIND | MS_REC, NULL))
		return -1;

	sprintf(path, "%s/%s", root, old_root);
	if (syscall(SYS_pivot_root, root, path))
		return -1;

	if (chdir("/"))
		return -1;

	sprintf(path, "/%s", old_root);
	if (umount2(path, MNT_DETACH))
		return -1;

	return 0;
}

static int map_user(int inside, int outside, int length, char const * mapfile)
{
	char mapping[100];
	sprintf(mapping, "%d %d %d", inside, outside, length);
	int fd = open(mapfile, O_WRONLY);
	if (fd < 0)
		return -1;

	if (write(fd, mapping, strlen(mapping)) < 0)
		return -1;

	close(fd);

	return 0;
}

int setup_user_ns(int pid)
{
	char path[100];
	sprintf(path, "/proc/%d/setgroups", pid);
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;

	if (write(fd, "deny", 4) < 0)
		return -1;

	sprintf(path, "/proc/%d/uid_map", pid);
	map_user(0, geteuid(), 1, path);
	sprintf(path, "/proc/%d/gid_map", pid);
	map_user(0, getegid(), 1, path);

	close(fd);

	return 0;
}

int setup_uts_ns(char const * hostname)
{
	return sethostname(hostname, strlen(hostname));
}

#define HOST_VETH_NAME "%dhost"
#define CONT_VETH_NAME "%dcont"

int create_host_cont_veth(int pid, char const * host_ip, char const * cont_ip)
{
	char cmd[100];

	sprintf(cmd, "sudo ip link add "HOST_VETH_NAME" type veth peer name "CONT_VETH_NAME, pid, pid);
	if (system(cmd))
		return -1;

	sprintf(cmd, "sudo ip link set "CONT_VETH_NAME" netns %d", pid, pid);
	if (system(cmd))
		return -1;

	return 0;
}

int up_veth_cont(int pid, char const * host_ip, char const * cont_ip)
{
	char cmd[100];

	sprintf(cmd, "ip link set lo up");
	if (system(cmd))
		return -1;

	sprintf(cmd, "ip addr add %s/24 dev "CONT_VETH_NAME, cont_ip, pid);
	if (system(cmd))
		return -1;

	sprintf(cmd, "ip link set "CONT_VETH_NAME" up", pid);
	if (system(cmd))
		return -1;

	sprintf(cmd, "ip route add default via %s", host_ip);
	if (system(cmd))
		return -1;

	return 0;
}

int up_veth_host(int pid, char const * ip)
{
	char cmd[100];

	sprintf(cmd, "sudo ip addr add %s/24 dev "HOST_VETH_NAME, ip, pid);
	if (system(cmd))
		return -1;

	sprintf(cmd, "sudo ip link set "HOST_VETH_NAME" up", pid);
	if (system(cmd))
		return -1;

	return 0;
}

int enter_ns(int pid, char const * nsname)
{
	char path[100];
	sprintf(path, "/proc/%d/ns/%s", pid, nsname);
	int nsfd = open(path, 0);
	if (nsfd < 0)
		return -1;

	if (setns(nsfd, 0))
		return -1;

	close(nsfd);

	return 0;
}
