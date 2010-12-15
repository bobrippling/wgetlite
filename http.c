#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "http.h"
#include "progress.h"
#include "util.h"

#define BSIZ 1024

void http_free_lines(char **lines)
{
	char **line;

	for(line = lines; *line; line++)
		free(*line);

	free(lines);
}

char **http_read_lines(int sock)
{
	int nlines = 10, curline = 0, eat_amount = 0;
	char **lines = malloc(nlines * sizeof(char *));
	char buffer[BSIZ];

	do{
		char *pos;

#define RECV(len, flags) \
		switch(recv(sock, buffer, len, flags)){ \
			case -1: \
			case  0: /* unexpected here - need \r\n\r\n */ \
				perror("recv()"); \
				http_free_lines(lines); \
				return NULL; \
		}

		RECV(sizeof buffer, MSG_PEEK);

		if((pos = strstr(buffer, "\r\n"))){
			*pos = '\0';
			if(!*buffer){
				RECV(2, 0); /* nom */
				lines[curline] = NULL;
				return lines;
			}

			if(!(lines[curline++] = strdup(buffer))){
				perror("strdup()");
				http_free_lines(lines);
				return NULL;
			}

			if(curline == nlines){
				char **new;
				nlines += 10;
				new = realloc(lines, nlines * sizeof(char *));
				if(!new){
					perror("realloc()");
					http_free_lines(lines);
					return NULL;
				}
				lines = new;
			}

			eat_amount = (pos - buffer) + 2;

			RECV(eat_amount, 0);
		}
	}while(1);
}

char *http_GET_find_line(char **lines, char *line)
{
	for(; *lines; lines++){
		register int len = strlen(line);
		if(!strncmp(*lines, line, len))
			return *lines + len;
	}
	return NULL;
}

int http_GET_recv(int sock, FILE *f, const char *fname)
{
#define RET(n) do{ ret = n; goto fin; }while(0)
	char **lines = http_read_lines(sock);
	char *slen;
	ssize_t len = -1, sofar = 0;
	long last_progress;
	int ret;

	if(!lines)
		return 1;

	fprintf(stderr, "HTTP: reply: %s\n", *lines);

	if((slen = http_GET_find_line(lines, "Content-Length: "))){
		if(sscanf(slen, "%ld", &len) == 1)
			fprintf(stderr, "HTTP: content-length: %ld\n", len);
		else
			len = 1;
	}else{
		fputs("HTTP: no content-length found\n", stderr);
		last_progress = mstime();
		progress_unknown();
	}

	http_free_lines(lines);

	do{
		ssize_t nread;
		char buffer[BSIZ];

		switch((nread = recv(sock, buffer, sizeof buffer, 0))){
			case -1:
				perror("recv()");
				RET(1);
			case 0:
				RET(0);

			default:
				if(!fwrite(buffer, sizeof(buffer[0]), nread, f)){
					perror("fwrite()");
					RET(1);
				}
		}

		if(len != -1){
			sofar += nread;
			progress_show(sofar, len, fname);
		}else{
			long t = mstime();

			if(last_progress + 250 < t){
				last_progress = t;
				progress_unknown();
			}
		}
	}while(1);

fin:
	progress_fin();
	return ret;
}

int http_GET(int sock, const char *file, FILE *out)
{
	char buffer[1024];

	snprintf(buffer, sizeof buffer,
			"GET %s HTTP/1.0\r\n\r\n", file);

	switch(write(sock, buffer, strlen(buffer))){
		case  0:
		case -1:
			perror("write()");
			return 1;
	}

	return http_GET_recv(sock, out, file);
}
