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

void http_free_lines(char **lines)
{
	char **line;

	for(line = lines; *line; line++)
		free(*line);

	free(lines);
}

char **http_read_lines(int sock)
{
	int nlines = 10, curline = 0;
	char **lines = malloc(nlines * sizeof(char *));

	if(!lines){
		perror("malloc()");
		return NULL;
	}

	lines[0] = NULL;

	do{
		if(!(lines[curline] = readline(sock))){
			perror("readline()");
			http_free_lines(lines);
			return NULL;
		}

		if(++curline == nlines){
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

		if(!*lines[curline-1])
			break;
	}while(1);
	return lines;
}

char *http_GET_find_line(char **lines, char *line)
{
	register int len = strlen(line);
	for(; *lines; lines++){
		if(!strncmp(*lines, line, len))
			return *lines + len;
	}
	return NULL;
}

int http_GET_recv(int sock, FILE *f, const char *fname)
{
	char **lines = http_read_lines(sock);
	char *slen;
	size_t len;

	if(!lines)
		return 1;

	fprintf(stderr, "HTTP: reply: %s\n", *lines);

	if((slen = http_GET_find_line(lines, "Content-Length: ")))
		if(sscanf(slen, "%zu", &len) == 1)
			fprintf(stderr, "HTTP: content-length: %ld\n", len);
		else
			len = 0;
	else
		fputs("HTTP: no content-length found\n", stderr);

	http_free_lines(lines);

	return generic_transfer(sock, f, fname, len);
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
