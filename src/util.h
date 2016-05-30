#ifndef UTIL_H
#define UTIL_H

struct config {
	char *port;
	char *root_dir;
	int backlog;
};

int check_digit(const char *str);
int is_folder(const char *str);

int init(int argc, char **argv, struct config *config);

void sigchld_handler(void);
void setup_signals(void);
int is_running(void);
#endif
