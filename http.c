#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "output.h"
#include "wgetlite.h"
#include "util.h"

#include "http.h"
#include "progress.h"


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

int http_recv(struct wgetfile *finfo, FILE *f)
{
	extern struct cfg global_cfg;
	char **lines = http_read_lines(finfo->sock);
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

	/* TODO: http_GET_find_line(lines, "Context-Disposition: ...") */

	if(400 <= http_code && http_code < 600)
		goto die;
	else
		switch(http_code){
			case HTTP_OK:
				if(global_cfg.partial){
					output_err(OUT_WARN, "HTTP: Partial transfer not supported");
					fclose(f);
					f = wget_open(finfo, "w");
					if(!f)
						goto die;
				}
				break;

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

					wget_close(finfo, f);
					ret = wget(location);
					http_free_lines(lines);

					return ret;
				}else
					output_err(OUT_WARN, "HTTP: Couldn't find Location header, continuing");

				break;
			}
		}

	len = 0;
	if((slen = http_GET_find_line(lines, "Content-Length: ")))
		if(sscanf(slen, "%zu", &len) == 1)
			output_err(OUT_INFO, "HTTP: Content-Length: %ld", len);
		else
			output_err(OUT_WARN, "HTTP: Content-Length unparseable");
	else
		output_err(OUT_INFO, "HTTP: No Content-Length header");

	http_free_lines(lines);

	return generic_transfer(finfo, f, len, ftell(f));
die:
	http_free_lines(lines);
	return 1;
}

int http_GET(struct wgetfile *finfo)
{
	extern struct cfg global_cfg;
	char buffer[1024];
	FILE *f;
	long pos;

	/* FIXME: check length */
	snprintf(buffer, sizeof buffer,
			"GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
			finfo->host_file, finfo->host_name);
	/* TODO: User-Agent: wgetlite/0.9 */

	f = wget_open(finfo, NULL);
	if(!f)
		return 1;

	if(global_cfg.partial && (pos = ftell(f)) > 0){
		/* FIXME: make sure we have "Accept-Ranges: bytes" header first? */
		char *append = strchr(buffer, '\0');
		append -= 2;

		snprintf(append, sizeof(buffer) - strlen(buffer) - 1,
				"Range: bytes=%ld-\r\n\r\n", pos);
	}

	output_err(OUT_VERBOSE, "HTTP: Request: GET %s HTTP/1.1 (Host: %s)", finfo->host_file, finfo->host_name);

	/* TODO: printf("HTTP request sent, awaiting response..."); */

	switch(write(finfo->sock, buffer, strlen(buffer))){
		case  0:
		case -1:
			output_perror("write()");
			return 1;
	}

	return http_recv(finfo, f);
}
