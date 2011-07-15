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
#include "connections.h"


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
			if(!curline){
				free(lines);
				return NULL;
			}
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

	if(!lines){
		output_err(OUT_ERR, "Premature end-of-stream");
		goto die;
	}

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

	len_transfer = 0;
	if((hdr = http_GET_find_line(lines, "Content-Length: ")) && sscanf(hdr, "%zu", &len_transfer) != 1)
		output_err(OUT_WARN, "HTTP: Content-Length unparseable");

	if(400 <= http_code && http_code < 600)
		if(http_code == HTTP_NOT_SATISFIABLE){
			output_err(OUT_INFO, "HTTP: Already fully downloaded");
			return 0;
		}else{
			goto die;
		}
	else
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

					if(len_transfer)
						connection_discard_data(finfo->sock, len_transfer);
					/* else no Content-Length, assume zero */


					/* don't close the socket, just follow through (eurgh) */
					ret = wget(to, finfo->redirect_no + 1);

					free(to);
					http_free_lines(lines);

					return ret;
				}else
					output_err(OUT_WARN, "HTTP: Couldn't find Location header, continuing");

				break;
			}
		}

	pos = ftell(f);
	if(pos == -1)
		pos = 0;

	if(len_transfer)
		output_err(OUT_INFO, "HTTP: Content-Length: %ld", len_transfer);
	else
		output_err(OUT_INFO, "HTTP: No Content-Length header");

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
	if(lines)
		http_free_lines(lines);
	if(len_transfer)
		connection_discard_data(finfo->sock, len_transfer);
	wget_remove_if_empty(finfo, f);
	return 1;
}

int http_print_lines(char **list, int fd, int n)
{
	int i;
	for(i = 0; i < n; i++)
		if(list[i])
			if(fdprintf(fd, "%s\r\n", list[i]) == -1)
				return 1;

	if(write(fd, "\r\n", 2) != 2)
		return 1;

	return 0;
}

int http_GET(struct wgetfile *finfo)
{
	extern struct cfg global_cfg;
	int sock_close = 0;
	FILE *f;
	long pos;
	int nheaders;
	char **headers;
	int hdidx;

	nheaders = 16;
	headers = malloc(nheaders * sizeof(char *));

	if(!headers){
		output_perror("malloc()");
		return 1;
	}

	headers[0] = allocprintf("HEAD %s HTTP/1.1", finfo->host_file);
	headers[1] = allocprintf("Host: %s", finfo->host_name);
	headers[2] = strdup("Connection: Keep-Alive");
	hdidx = 3;

	f = wget_open(finfo, NULL);
	if(!f)
		return 1;

	output_err(OUT_VERBOSE, "HTTP: Request: GET %s HTTP/1.1 (Host: %s)",
			finfo->host_file, finfo->host_name);


	if(global_cfg.partial && (pos = ftell(f)) > 0)
		headers[hdidx++] = allocprintf("Range: bytes=%ld-", pos);
		/*
		 * no need to check for "Accept-Ranges: bytes" header
		 * since we can check for 200-OK later on
		 */


	if(global_cfg.user_agent){
		headers[hdidx++] = allocprintf("User-Agent: %s", global_cfg.user_agent);
		output_err(OUT_DEBUG, "HTTP: User Agent \"%s\"", global_cfg.user_agent);
	}


	{
		struct cookie *c, *start;

		for(start = c = cookies_get(finfo->host_name); c; c = c->next){
			output_err(OUT_DEBUG, "HTTP: Cookie: %s=%s", c->nam, c->val);
			headers[hdidx++] = allocprintf("Cookie: %s=%s", c->nam, c->val);
			if(hdidx == nheaders-2){
				output_err(OUT_WARN, "Too many cookies");
				break;
			}
		}

		cookies_free(start, 0);
	}

	headers[hdidx] = NULL;

	{
		char **lines;
		char *ka;

		if(http_print_lines(headers, finfo->sock, hdidx)){
			http_free_lines(headers);
			return 1;
		}

		lines = http_read_lines(finfo->sock);

		if(!lines){
			http_free_lines(lines);
			http_free_lines(headers);
			return 1;
		}

		ka = http_GET_find_line(lines, "Connection: ");

		if(!ka || !strcasecmp(ka, "close"))
			sock_close = 1;

		ka = http_GET_find_line(lines, "Content-Length: ");
		if(sock_close || !ka){
			/* we can't do keep-alive, since we don't know the amount to skip ahead etc etc */
			free(headers[2]);
			headers[2] = strdup("Connection: close");
		}

		http_free_lines(lines);
	}

	free(headers[0]);
	headers[0] = allocprintf("GET %s HTTP/1.1", finfo->host_file);

	if(sock_close){
		connection_close_fd(finfo->sock);
		if(wget_connect(finfo))
			return 1;
	}

	http_print_lines(headers, finfo->sock, hdidx);
	http_free_lines(headers);

	return http_recv(finfo, f);
}
