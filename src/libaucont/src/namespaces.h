#ifndef NAMESPACES_H
#define NAMESPACES_H

int setup_mount_ns(char const * root);

int setup_user_ns(int pid);

int setup_uts_ns(char const * hostname);

int create_host_cont_veth(int pid, char const * host_ip, char const * cont_ip);

int up_veth_cont(int pid, char const * host_ip, char const * cont_ip);

int up_veth_host(int pid, char const * ip);

int enter_ns(int pid, char const * nsname);

#endif	// NAMESPACES_H
