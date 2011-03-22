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
#include "output.h"

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
		output_perror("malloc()");
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
				output_perror("realloc()");
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

int http_recv(int sock, FILE **f, const char *fname, size_t fpos)
{
	extern struct cfg global_cfg;
	char **lines = http_read_lines(sock);
	char *slen;
	size_t len;
	int http_code;

	if(!lines)
		return 1;

	if(sscanf(*lines, "HTTP/1.%*d %d", &http_code) != 1){
		output_err(OUT_WARN, "HTTP: Couldn't parse HTTP response code");
		http_code = HTTP_OK;
	}

	if(global_cfg.verbosity <= OUT_VERBOSE){
		char **iter;
		for(iter = lines; *iter; iter++)
			output_err(OUT_VERBOSE, "HTTP: Header: %s", *iter);
	}else
		output_err(OUT_INFO, "HTTP: reply: %s", *lines);


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

					output_err(OUT_INFO, "HTTP: Location redirect, following - %s", location);

					if(*f != stdout)
						fclose(*f);
					*f = NULL; /* tell the caller that we'll handle it */

					ret = wget(location);
					http_free_lines(lines);

					return ret;
				}else
					output_err(OUT_WARN, "Couldn't find Location header, continuing");

				break;
			}
		}

	len = 0;
	if((slen = http_GET_find_line(lines, "Content-Length: ")))
		if(sscanf(slen, "%zu", &len) == 1)
			output_err(OUT_INFO, "HTTP: Content-Length: %ld", len);
		else
			output_err(OUT_WARN, "HTTP Content-Length unparseable");
	else
		output_err(OUT_INFO, "HTTP: No Content-Length header");

	http_free_lines(lines);

	return generic_transfer(sock, *f, fname, len, fpos);
die:
	http_free_lines(lines);
	return 1;
}

int http_GET(int sock, const char *file, FILE **out, size_t fpos)
{
	extern struct cfg global_cfg;
	char buffer[1024];

	snprintf(buffer, sizeof buffer, "GET %s HTTP/1.0\r\n\r\n", file);

	/* TODO: User-Agent: wgetlite/0.9 */
	if(global_cfg.partial && fpos){
		char *append = strchr(buffer, '\0');
		append -= 2;

		snprintf(append, sizeof(buffer) - strlen(buffer) - 1,
				"Range: bytes=%ld-\r\n\r\n", fpos);
	}


	/* TODO: printf("HTTP request sent, awaiting response..."); */

	switch(write(sock, buffer, strlen(buffer))){
		case  0:
		case -1:
			output_perror("write()");
			return 1;
	}

	return http_recv(sock, out, file, fpos);
}
