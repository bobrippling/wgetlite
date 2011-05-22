#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "output.h"
#include "wgetlite.h"
#include "util.h"

#include "http.h"
#include "progress.h"
#include "cookies.h"


#define HTTP_OK                    200
#define HTTP_PARTIAL_CONTENT       206

#define HTTP_MOVED_PERMANENTLY     301
#define HTTP_FOUND                 302
#define HTTP_SEE_OTHER             303
#define HTTP_TEMPORARY_REDIRECT    307

#define HTTP_CLIENT_ERR            400
#define HTTP_NOT_SATISFIABLE       416

#define HTTP_SVR_ERR               500


#define HTTP_MAX_REDIRECTS          20


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
		if(!(lines[curline] = readline(sock))){
			http_free_lines(lines);
			return NULL;
		}

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
	int len = strlen(line);

	for(; *lines; lines++){
		if(!strncasecmp(*lines, line, len))
			return *lines + len;
	}
	return NULL;
}

int http_recv(struct wgetfile *finfo, FILE *f)
{
	extern struct cfg global_cfg;
	char **lines;
	char *hdr;
	ssize_t pos;
	size_t len_transfer;
	int http_code;

	lines = http_read_lines(finfo->sock);

	if(!lines)
		return 1;

	if(sscanf(*lines, "HTTP/1.%*d %d ", &http_code) != 1 &&
			sscanf(*lines, "HTTP %d ", &http_code) != 1){
		output_err(OUT_WARN, "HTTP: Warning: Couldn't parse HTTP response code");
		http_code = HTTP_OK;
	}

	if(global_cfg.verbosity <= OUT_VERBOSE){
		char **iter;
		for(iter = lines; *iter; iter++)
			output_err(OUT_VERBOSE, "HTTP: Header: %s", *iter);
	}else
		output_err(OUT_INFO, "HTTP: Reply: %s", *lines);


	if(finfo->namemode == NAME_GUESS &&
			(hdr = http_GET_find_line(lines, "Content-Disposition: "))){
		char *name = strstr(hdr, "filename=\"");

		if(name){
			char *end;
			name += 10;
			end = strchr(name, '"');
			if(end){
				*end = '\0';
				output_err(OUT_INFO, "HTTP: Using server name: \"%s\"", name);
				/* FIXME: relink to name */
			}else
				name = NULL;
		}
	}


	if(400 <= http_code && http_code < 600){
		if(http_code == HTTP_NOT_SATISFIABLE){
			output_err(OUT_INFO, "HTTP: Already fully downloaded");
			return 0;
		}else
			goto die;
	}else
		switch(http_code){
			case HTTP_OK:
				if(global_cfg.partial && ftell(f) > 0){
					output_err(OUT_ERR, "HTTP: Partial transfer rejected (200 OK)");
					goto die;
				}
				break;

			case HTTP_PARTIAL_CONTENT:
				break;

			case HTTP_MOVED_PERMANENTLY:
			case HTTP_FOUND:
			case HTTP_SEE_OTHER:
			case HTTP_TEMPORARY_REDIRECT:
			{
				/* redo the request with the new data */
				const char *location = http_GET_find_line(lines, "Location: ");
				if(location){
					char *to;
					int ret;
					int len = strlen(location) + strlen(finfo->host_port) + 2; /* plus an extra for ':' */

					if(finfo->redirect_no > HTTP_MAX_REDIRECTS){
						output_err(OUT_ERR, "HTTP: Max Redirects (%d) Reached", HTTP_MAX_REDIRECTS);
						return 1;
					}

					to = malloc(len);
					if(!to){
						output_perror("malloc()");
						return 1;
					}

					if(*location == '/'){
						/* need to prepend host:port */
						char *new = realloc(to, len += strlen(finfo->host_name));

						if(!new){
							output_perror("realloc()");
							goto die;
						}
						to = new;

						snprintf(to, len, "%s:%s%s", finfo->host_name, finfo->host_port, location);
					}else{
						/* if port already given, go with it, else assume current port */
						if(strchr(location, ':'))
							strcpy(to, location);
						else
							snprintf(to, len, "%s:%s", location, finfo->host_port);
					}

					output_err(OUT_INFO, "HTTP: Location redirect, following - %s", to);

					wget_close(finfo, f);
					wget_remove(finfo);

					close(finfo->sock);
					finfo->sock = -1;

					ret = wget(to, finfo->redirect_no + 1);

					free(to);
					http_free_lines(lines);

					return ret;
				}else
					output_err(OUT_WARN, "HTTP: Couldn't find Location header, continuing");

				break;
			}
		}

	len_transfer = 0;
	if((hdr = http_GET_find_line(lines, "Content-Length: ")))
		if(sscanf(hdr, "%zu", &len_transfer) == 1)
			output_err(OUT_INFO, "HTTP: Content-Length: %ld", len_transfer);
		else
			output_err(OUT_WARN, "HTTP: Content-Length unparseable");
	else
		output_err(OUT_INFO, "HTTP: No Content-Length header");

	pos = ftell(f);
	if(pos == -1)
		pos = 0;

	/* len is the amount to be sent during this transmission */
	if(http_code == HTTP_PARTIAL_CONTENT){
		if((hdr = http_GET_find_line(lines, "Content-Range: "))){
			size_t start, stop, total;

			if(sscanf(hdr, "bytes %zu-%zu/%zu", &start, &stop, &total) == 3){
				if(total == stop + 1){
					if(pos == (signed)start){
						len_transfer = stop - start + 1;

						if(len_transfer && start + len_transfer != total)
							output_err(OUT_WARN, "HTTP: Server reports different content-length and content-range");

						output_err(OUT_INFO, "HTTP: Resuming transfer at %zu, for %zu bytes, total length %zu",
								pos, len_transfer, total);
					}else{
						output_err(OUT_ERR, "HTTP: Server resuming at wrong position");
						goto die;
					}
				}else{
					output_err(OUT_ERR, "HTTP: Unsupported Content-Range");
					goto die;
				}
			}else
				output_err(OUT_WARN, "HTTP: Couldn't parse Content-Range \"%s\"", hdr);
		}else if(global_cfg.partial)
			output_err(OUT_WARN, "HTTP: No Content-Range header");
	}

	http_free_lines(lines);

	return generic_transfer(finfo, f, pos + len_transfer, pos);
die:
	http_free_lines(lines);
	wget_remove_if_empty(finfo, f);
	return 1;
}

int http_GET(struct wgetfile *finfo)
{
	extern struct cfg global_cfg;
	FILE *f;
	long pos;

#define FDPRINTF(...) \
		do \
			if(fdprintf(__VA_ARGS__) == -1){ \
				output_perror("send()"); \
				return 1; \
			} \
		while(0)

	FDPRINTF(finfo->sock, "GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: close\r\n",
			finfo->host_file, finfo->host_name);

	f = wget_open(finfo, NULL);
	if(!f)
		return 1;

	output_err(OUT_VERBOSE, "HTTP: Request: GET %s HTTP/1.1 (Host: %s)",
			finfo->host_file, finfo->host_name);


	if(global_cfg.partial && (pos = ftell(f)) > 0)
		FDPRINTF(finfo->sock, "Range: bytes=%ld-\r\n", pos);
		/*
		 * no need to check for "Accept-Ranges: bytes" header
		 * since we can check for 200-OK later on
		 */


	if(global_cfg.user_agent){
		FDPRINTF(finfo->sock, "User-Agent: %s\r\n", global_cfg.user_agent);
		output_err(OUT_DEBUG, "HTTP: User Agent \"%s\"", global_cfg.user_agent);
	}


	{
		struct cookie *c, *start;

		for(start = c = cookies_get(finfo->host_name); c; c = c->next){
			output_err(OUT_DEBUG, "HTTP: Cookie: %s=%s", c->nam, c->val);
			FDPRINTF(finfo->sock, "Cookie: %s=%s\r\n", c->nam, c->val);
		}

		cookies_free(start, 0);
	}

	FDPRINTF(finfo->sock, "\r\n");

	return http_recv(finfo, f);
}
