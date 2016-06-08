#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "http.h"
#include "util.h"

struct mime_type {
	const char *extension;
	const char *mime_type;
} mime_types[] = {
	#include "mime_types.h"
};

void parse_header(int sock, struct http_header *header)
{
	int i = 0;
	int j = 0;
	int c = '\n';
	memset(header, 0, sizeof(struct http_header));
	char *buf = getcwd(header->uri, URI_STRLEN);
	for (i = 0; (i < METHOD_STRLEN) && (recv(sock, &c, 1, 0) > 0) && (c != ' ' && c != '\n'); i++) {
		if (c != '\r') {
			header->method[i] = c;
		}
	}
	for (i = strlen(header->uri), j = i; (i < URI_STRLEN) && (recv(sock, &c, 1, 0) > 0) && (c != ' ' && c != '\n'); i++) {
		if (c != '\r') {
			header->uri[i] = c;
		}
	}

	for (i = 0; (i < VERSION_STRLEN) && (recv(sock, &c, 1, 0) > 0) && (c != ' ' && c != '\n'); i++) {
		if (c != '\r') {
			header->version[i] = c;
		}
	}
}

int request_method(int sock, const char *method)
{
	if (!strcmp(method, "GET")) {
		return 0;
	} else if (!strcmp(method, "POST")) {
		send_error(sock, HTTP_NOT_IMPLEMENTED);
	} else {
		send_error(sock, HTTP_BAD_REQUEST);
	}
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return -1;
}

void send_line(int sock, const char *format, ...)
{
	char *str;
	va_list list;
	int size;
	va_start(list, format);
	if ((size = vasprintf(&str, format, list)) < 0) {
		return;
	}
	va_end(list);
	send(sock, str, size, 0);
	free(str);
}

void send_error(int sock, int errcode)
{
	char *buf;
	switch (errcode) {
	case HTTP_BAD_REQUEST: {
		send_line(sock, "HTTP/1.1 %d Bad Request\r\n", errcode);
		send_line(sock, "Content-type: text/html\r\n");
		send_line(sock, "\r\n");
		send_line(sock, "<html><title>%d Bad Request</title><body><h1>%d Bad Request</h1><p>Your browser sent an invalid request!</p></body></html>", errcode, errcode);
		break;
	}
	case HTTP_FORBIDDEN: {
		send_line(sock, "HTTP/1.1 %d Forbidden\r\n", errcode);
		send_line(sock, "Content-Type: text/html\r\n");
		send_line(sock, "\r\n");
		send_line(sock, "<html><title>%d Forbidden</title><body><h1>%d Forbidden</h1><p>You don't have access to this content. Check your priviledge!</p></body></html>", errcode, errcode);
		break;
	}
	case HTTP_NOT_FOUND: {
		send_line(sock, "HTTP/1.1 %d Not Found\r\n", errcode);
		send_line(sock, "Content-Type: text/html\r\n");
		send_line(sock, "\r\n");
		send_line(sock, "<html><title>%d Not Found</title><body><h1>%d Not Found</h1><p>The server could not find the specified resource!</p></body></html>", errcode, errcode);
		break;
	}
	case HTTP_NOT_IMPLEMENTED: {
		send_line(sock, "HTTP/1.1 %d Not Implemented\r\n", errcode);
		send_line(sock, "Content-Type: text/html\r\n");
		send_line(sock, "\r\n");
		send_line(sock, "<html><title>%d Not Implemented</title><body><h1>%d Not Implemented</h1><p>This server has not implemented the requested method!</p></body></html>", errcode, errcode);
		break;
	}
	case HTTP_INTERNAL_ERROR:
	default:
		send_line(sock, "HTTP/1.1 %d Internal Server Error\r\n", HTTP_INTERNAL_ERROR);
		send_line(sock, "Content-Type: text/html\r\n");
		send_line(sock, "\r\n");
		send_line(sock, "<html><title>%d Internal Server Error</title><body><h1>%d Internal Server Error</h1><p>Oops!</p></body></html>", HTTP_INTERNAL_ERROR, HTTP_INTERNAL_ERROR);
		break;
	}
}

const char* get_content_type(const char *uri)
{
	int i;
	char *point;
	if ((point = strrchr(uri, '.')) != NULL) {
		point++;
		for (i = 0; i < sizeof(mime_types)/sizeof(*mime_types); i++) {
			if (!strcmp(point, mime_types[i].extension)) {
					return mime_types[i].mime_type;
			}
		}
	}
	return "text/plain; charset=utf-8";
}
void send_file(int sock, const char *uri)
{
	struct stat st;
	int fd = open(uri, O_RDONLY, 0);
	if (fd < 0) {
		perror("open");
		send_error(sock, HTTP_NOT_FOUND);
		return;
	}
	send_line(sock, "HTTP/1.1 200 OK\r\n");
	send_line(sock, "Content-Type: %s\r\n", get_content_type(uri));
	send_line(sock, "\r\n");
	fstat(fd, &st);
	if (sendfile(sock, fd, NULL, st.st_size) < 0) {
		perror("sendfile");
	}
	close(fd);
}

void send_dir(int sock, const char *uri)
{
	char *index;
	char ltime[40];
	int size = asprintf(&index, "%s/index.html", uri);
	if (file_exists(index) != -1) {
		send_file(sock, index);
	} else {
		int dir_fd = open(uri, O_DIRECTORY);
		if (dir_fd < 0) {
			perror("open");
		}
		send_line(sock, "HTTP/1.1 200 OK\r\n%s%s%s%s%s",
			  "Content-Type: text/html\r\n\r\n",
			  "<html><head><style>",
			  "body{font-family: monospace; font-size: 13px;}",
			  "td {padding: 1.5px 6px;}",
			  "</style></head><body><table>\n");
		DIR *d = fdopendir(dir_fd);
		struct dirent *dp;
		struct stat st;
		int fd;
		while ((dp = readdir(d)) != NULL) {
			if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
				continue;
			} else if ((fd = openat(dir_fd, dp->d_name, O_RDONLY)) == -1) {
				perror(dp->d_name);
				continue;
			}
			fstat(fd, &st);
			strftime(ltime, sizeof(ltime),
				 "%Y-%m-%d %H:%M", localtime(&st.st_mtime));
			if (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) {
				char *ext = S_ISREG(st.st_mode) ? "" : "/";
				char *fsize;
				file_size(&fsize, st);
				send_line(sock, "<tr><td><a href=\"%s%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>\n", basename(uri), dp->d_name, ext, dp->d_name, ext, ltime, fsize);
				free(fsize);
			}
			close(fd);
		}
		send_line(sock, "</table></body></html>");
		closedir(d);
	}
	free(index);
}

void handle_connection(int sock)
{
	printf("fd: %d, pid: %d\n", sock, getpid());
	struct http_header header;
	struct stat st;
	parse_header(sock, &header);
	if (request_method(sock, header.method) < 0) {
		send_error(sock, HTTP_NOT_IMPLEMENTED);
	} else if (stat(header.uri, &st) < 0) {
		send_error(sock, errno == EACCES ? HTTP_FORBIDDEN : HTTP_NOT_FOUND);
	} else if (S_ISREG(st.st_mode)) {
		send_file(sock, header.uri);
	} else if (S_ISDIR(st.st_mode)) {
		send_dir(sock, header.uri);
	} else {
		send_error(sock, HTTP_INTERNAL_ERROR);
	}
	shutdown(sock, SHUT_RDWR);
	close(sock);
}
