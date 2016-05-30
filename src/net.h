#ifndef NET_H
#define NET_H

#include <sys/socket.h>

void *get_in_addr(struct sockaddr *sa);
int get_sock(const char *port, int *sock);

#endif
