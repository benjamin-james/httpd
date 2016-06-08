#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <getopt.h>
#include <regex.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

static volatile sig_atomic_t _running = 1;
int is_running(void)
{
	return _running != 0 ? 1 : 0;
}

void stop_running(int signum)
{
	_running = 0;
}

int check_digit(const char *msg)
{
	regex_t regex;
	int r = regcomp(&regex, "^[[:digit:]]+$", REG_EXTENDED);
	if (r) {
		fprintf(stderr, "Could not compile regex\n");
		exit(EXIT_FAILURE);
	}
	r = regexec(&regex, msg, 0, NULL, 0);
        if (r == REG_NOMATCH) {
		fprintf(stderr, "No match for %s\n", msg);
		return -1;
	} else if (r != 0) {
		char msgbuf[256];
		regerror(r, &regex, msgbuf, sizeof(msgbuf));
		fprintf(stderr, "Regex match failed: %s\n", msgbuf);
		exit(EXIT_FAILURE);
	}
	regfree(&regex);
	return 0;
}

int file_size(char **str, struct stat st)
{
	if (S_ISDIR(st.st_mode)) {
		return asprintf(str, "[DIR]");
	} else if (st.st_size < 1024) {
		return asprintf(str, "%lu", st.st_size);
	} else if (st.st_size < 1024 * 1024) {
		return asprintf(str, "%.1fK", (double)st.st_size / 1024.0);
	} else if (st.st_size < 1024 * 1024 * 1024) {
		return asprintf(str, "%.1fM", (double)st.st_size / (1024.0 * 1024.0));
	} else {
		return asprintf(str, "%.1fG", (double)st.st_size / (1024.0 * 1024.0 * 1024.0));
	}
}

int file_exists(const char *str)
{
	struct stat st;
	return (stat(str, &st) == 0 && S_ISREG(st.st_mode)) ? 0 : -1;
}

int is_folder(const char *str)
{
	struct stat st;
	return (stat(str, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : -1;
}

int init(int argc, char **argv, struct config *status)
{
	status->port = NULL;
	status->root_dir = NULL;
	status->backlog = 1024;
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"port", required_argument, 0, 'p'},
		{"root-dir", required_argument, 0,  'r' },
		{"backlog", required_argument, 0, 'b'},
		{0, 0, 0, 0}
	};
	int c, option_index;
	while ((c = getopt_long(argc, argv, "hp:r:b:", long_options, &option_index)) != -1) {
		switch (c) {
		case 'h': {
			printf("Usage: %s [OPTIONS]\n", argv[0]);
			printf("\t--help    \tDisplay this message\n");
			printf("\t--port    \tSet the port of the server\n");
			printf("\t--root-dir\tSet the root directory of the server\n");
			printf("\t--backlog\tSet the max incoming connections to listen to\n");
			exit(EXIT_SUCCESS);
		}
		case 'b': {
				check_digit(optarg);
				status->backlog = (int)strtol(optarg, NULL, 10);
				break;
			}
		case 'p': {
			check_digit(optarg);
			status->port = strdup(optarg);
			break;
		}
		case 'r': {
			if (is_folder(optarg) == -1) {
				fprintf(stderr, "\"%s\" is not a directory\n", optarg);
				exit(EXIT_FAILURE);
			}
			if (status->root_dir) {
				free(status->root_dir);
			}
			status->root_dir = strdup(optarg);
			break;
		}
		default:
			break;
		}
	}
	if (status->port == NULL) {
		uid_t uid = geteuid();
		status->port = strdup(uid == 0 ? "80" : "8080");
	}
	if (status->root_dir == NULL) {
		status->root_dir = get_current_dir_name();
	}
	return 0;
}

void sigchld_handler(void)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_signals(void)
{
	struct sigaction sa;
	sa.sa_handler = (void *)sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler = stop_running;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}
