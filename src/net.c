#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_sock(const char *port, int *sock_ret)
{
	struct addrinfo *info, *p;
	int sock;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, port, &hints, &info);
	for (p = info; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
		if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
			close(sock);
			perror("bind");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "failed to bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(info);
	*sock_ret = sock;
	return 0;
}
