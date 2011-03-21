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

#include "wgetlite.h"

#define HTTP_OK                    200

#define HTTP_MOVED_PERMANENTLY     301
#define HTTP_FOUND                 302
#define HTTP_SEE_OTHER             303
#define HTTP_TEMPORARY_REDIRECT    307

#define HTTP_CLIENT_ERR            400

#define HTTP_SVR_ERR               500



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
		char *r;

		if(!(lines[curline] = readline(sock))){
			http_free_lines(lines);
			return NULL;
		}

		r = strchr(lines[curline], '\r');
		if(r)
			*r = '\0';

		if(!*lines[curline]){
			free(lines[curline]);
			lines[curline] = NULL;
			break;
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

int http_GET_recv(int sock, FILE **f, const char *fname)
{
	extern struct cfg global_cfg;
	char **lines = http_read_lines(sock);
	char *slen;
	size_t len;
	int http_code;

	if(!lines)
		return 1;

	if(sscanf(*lines, "HTTP/1.%*d %d", &http_code) != 1){
		fputs("Warning: Couldn't parse HTTP response code\n", stderr);
		http_code = HTTP_OK;
	}

	if(global_cfg.verbose){
		char **iter;
		for(iter = lines; *iter; iter++)
			fprintf(stderr, "HTTP: Header: %s\n", *iter);
	}else
		fprintf(stderr, "HTTP: reply: %s\n", *lines);


	if(400 <= http_code && http_code < 600)
		goto die;
	else
		switch(http_code){
			case HTTP_MOVED_PERMANENTLY:
			case HTTP_FOUND:
			case HTTP_SEE_OTHER:
			case HTTP_TEMPORARY_REDIRECT:
			{
				/* redo the request with the new data */
				const char *location = http_GET_find_line(lines, "Location: ");
				if(location){
					int ret;

					fprintf(stderr, "HTTP: Location redirect, following - %s\n", location);

					fclose(*f);
					*f = NULL;

					ret = wget(location);
					http_free_lines(lines);

					return ret;
				}else
					fputs("Couldn't find Location header, continuing\n", stderr);

				break;
			}
		}

	len = 0;
	if((slen = http_GET_find_line(lines, "Content-Length: ")))
		if(sscanf(slen, "%zu", &len) == 1)
			fprintf(stderr, "HTTP: Content-length: %ld\n", len);
		else
			fputs("HTTP Content-length unparseable\n", stderr);
	else
		fputs("HTTP: no Content-length header\n", stderr);

	http_free_lines(lines);

	return generic_transfer(sock, *f, fname, len);
die:
	http_free_lines(lines);
	return 1;
}

int http_GET(int sock, const char *file, FILE **out)
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
