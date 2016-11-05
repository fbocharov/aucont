#ifndef NAMESPACES_H
#define NAMESPACES_H

int setup_mount_ns(char const * root);

int setup_user_ns(int pid);

int setup_uts_ns(char const * hostname);

int enter_ns(int pid, char const * nsname);

#endif	// NAMESPACES_H
