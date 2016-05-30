#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include "net.h"
#include "util.h"
#include "http.h"


int main(int argc, char **argv)
{
	struct config status;
	int sock;
	init(argc, argv, &status);
	if (chdir(status.root_dir) < 0) {
		perror("chdir");
		exit(EXIT_FAILURE);
	}
	printf("port: %s\nroot: %s\nbacklog: %d\n", status.port, status.root_dir, status.backlog);
	get_sock(status.port, &sock);
	if (listen(sock, status.backlog) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	setup_signals();
	char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage their_addr;
	while (is_running()) {
		socklen_t sin_size = sizeof their_addr;
		int newsock = accept(sock, (struct sockaddr *)&their_addr, &sin_size);
		if (newsock == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("got connection from %s\n", s);
		if (!fork()) {
			if (chdir(status.root_dir) < 0) {
				perror("chdir");
			}
			close(sock);
			handle_connection(newsock);
			close(newsock);
			exit(EXIT_SUCCESS);
		}
		close(newsock);
	}
	free(status.root_dir);
	free(status.port);
	return 0;
}
