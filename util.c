#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sys/time.h>

#include "util.h"
#include "progress.h"

#define BSIZ 1024


long mstime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

char *strdup(const char *s)
{
	unsigned int siz = strlen(s) + 1;
	char *d = malloc(siz);
	if(!d){
		fprintf(stderr, "Couldn't allocate %ud bytes\n", siz);
		exit(1);
	}
	strcpy(d, s);
	return d;
}

FILE *fdup(FILE *f, char *mode)
{
	return fdopen(fileno(f), mode);
}

char *readline(int sock)
{
	char buffer[BSIZ];
	char *pos;

#define RECV(len, flags) \
	switch(recv(sock, buffer, len, flags)){ \
		case -1: \
			perror("recv()"); \
			return NULL; \
		case  0: /* unexpected here - need \r\n\r\n */ \
			fputs("premature EOF\n", stderr); \
			return NULL; \
	}

	RECV(sizeof buffer, MSG_PEEK);

	if((pos = strstr(buffer, "\r\n"))){
		int len = pos - buffer + 2;
		RECV(len, 0);
		*pos = '\0';

		return strdup(buffer);
	}

	fputs("util::readline(): buffer too small\n", stderr);
	return NULL;
}

int fdprintf(int fd, const char *fmt, ...)
{
	char buffer[512];
	va_list l;
	unsigned int n;

	va_start(l, fmt);
	n = vsnprintf(buffer, sizeof buffer, fmt, l);
	va_end(l);

	if(n >= sizeof(buffer))
		fputs("fdprintf() warning: buffer not large enough - FIXME\n",
				stderr);

	return write(fd, buffer, n);
}

int dial(const char *host, const char *port)
{
	struct addrinfo hints, *list = NULL, *iter;
	int sock, last_err;

	memset(&hints, '\0', sizeof hints);
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((last_err = getaddrinfo(host, port, &hints, &list))){
		fprintf(stderr, "getaddrinfo(): \"%s:%s\": %s\n", host, port, gai_strerror(last_err));
		return -1;
	}


	for(iter = list; iter; iter = iter->ai_next){
		sock = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);

		if(sock == -1)
			continue;

		if(connect(sock, iter->ai_addr, iter->ai_addrlen) == 0)
			break;

		if(errno)
			last_err = errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(list);

	if(sock == -1)
		errno = last_err;
	return sock;
}

int generic_transfer(int sock, FILE *out, const char *fname, size_t len)
{
#define RET(n) do{ ret = n; goto fin; }while(0)
	int ret = 0;
	size_t sofar = 0;
	long last_progress;

	(void)fname;

	last_progress = mstime();
	if(!len)
		progress_unknown();

	do{
		ssize_t nread;
		char buffer[BSIZ];
		long t;

		switch((nread = recv(sock, buffer, sizeof buffer, 0))){
			case -1:
				perror("recv()");
				RET(1);
			case 0:
				if(len){
					if(sofar == len)
						RET(0);
					else{
						/* FIXME: retry */
						progress_incomplete();
						RET(1);
					}
				}else
					/* no length, assume we have the whole file */
					RET(0);

			default:
				if(!fwrite(buffer, sizeof(buffer[0]), nread, out)){
					perror("fwrite()");
					RET(1);
				}
		}

		sofar += nread;

		t = mstime();
		if(last_progress + 100 < t){
			last_progress = t;

			if(len)
				progress_show(sofar, len);
			else
				progress_unknown();
		}
	}while(1);

fin:
	if(len)
		progress_fin(sofar, len);
	else
		progress_fin(0, 0);
	return ret;
}
