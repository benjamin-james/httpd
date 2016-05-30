#ifndef HTTP_H
#define HTTP_H

#include <limits.h>
#include <stdlib.h>

#define VERSION_STRLEN 10
#define METHOD_STRLEN 5
#define URI_STRLEN PATH_MAX

struct http_header {
	char version[VERSION_STRLEN];
	char method[METHOD_STRLEN];
	char uri[URI_STRLEN];
};

#define HTTP_BAD_REQUEST 400
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_NOT_IMPLEMENTED 501
#define HTTP_INTERNAL_ERROR 500

void handle_connection(int sock);
const char* get_content_type(const char *uri);
void parse_header(int sock, struct http_header *header);
int request_method(int sock, const char *method);
void send_error(int sock, int errcode);
void send_file(int sock, const char *uri);
void send_dir(int sock, const char *uri);
#endif
