#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include "namespaces.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

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

	if (mount(root, root, "bind", MS_BIND | MS_REC, NULL)) {
		perror("binding");
		return -1;
	}

	sprintf(path, "%s/%s", root, old_root);
	if (syscall(SYS_pivot_root, root, path)) {
		perror("syscall");
		return -1;
	}

	if (chdir("/")) {
		perror("chdir");
		return -1;
	}

	sprintf(path, "/%s", old_root);
	if (umount2(path, MNT_DETACH))
		return -1;

	return 0;
}

int map_user(int inside, int outside, int length, char const * mapfile)
{
	char mapping[100];
	sprintf(mapping, "%d %d %d", inside, outside, length);
	int fd = open(mapfile, O_WRONLY);
	if (!fd) {
		perror("open user mapping");
		return -1;
	}

	if (write(fd, mapping, strlen(mapping)) < 0) {
		perror("write user mapping");
		return -1;
	}

	close(fd);

	return 0;
}

int setup_user_ns(int pid)
{
	char path[100];
	sprintf(path, "/proc/%d/setgroups", pid);
	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		perror("open setgroups");
		return -1;
	}

	if (write(fd, "deny", 4) < 0) {
		perror("write setgroups");
		return -1;
	}
	close(fd);

	sprintf(path, "/proc/%d/uid_map", pid);
	map_user(0, geteuid(), 1, path);
	sprintf(path, "/proc/%d/gid_map", pid);
	map_user(0, getegid(), 1, path);

	return 0;
}

int setup_uts_ns(char const * hostname)
{
	return sethostname(hostname, strlen(hostname));
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
