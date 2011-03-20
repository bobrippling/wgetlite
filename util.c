#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>


#include <sys/time.h>
#include <sys/socket.h>

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
	char *d = malloc(strlen(s) + 1);
	if(d)
		strcpy(d, s);
	return d;
}

char *readline(int sock)
{
	char buffer[BSIZ];
	char *pos;

#define RECV(len, flags) \
	switch(recv(sock, buffer, len, flags)){ \
		case -1: \
		case  0: /* unexpected here - need \r\n\r\n */ \
			perror("recv()"); \
			return NULL; \
	}

	RECV(sizeof buffer, MSG_PEEK);

	if((pos = strstr(buffer, "\r\n"))){
		int len;

		*pos = '\0';
		len = 1 + strlen(buffer);

		RECV(len, 0);

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

int dial(const char *host, int port)
{
#define CHECK(f, s) \
	if(f){ \
		perror(s "()"); \
		return -1; \
	}
	struct sockaddr_in addr;
	struct hostent *hent;
	int sock;

	if(!(hent = gethostbyname(host))){
		fprintf(stderr, "gethostbyname(): %s\n", strerror(h_errno));
		return -1;
	}

	CHECK((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1, "socket");

	memset(&addr, 0, sizeof addr);
	memcpy(&addr.sin_addr.s_addr, hent->h_addr_list[0], sizeof addr.sin_addr.s_addr);
	addr.sin_port   = htons(port);
	addr.sin_family = AF_INET;

	CHECK(connect(sock, (struct sockaddr *)&addr, sizeof addr) == -1, "connect");

	return sock;
}

int generic_transfer(int sock, FILE *out, const char *fname, size_t len)
{
#define RET(n) do{ ret = n; goto fin; }while(0)
	int ret = 0;
	size_t sofar = 0;
	long last_progress;

	if(!len){
		last_progress = mstime();
		progress_unknown();
	}

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
				if(!fwrite(buffer, sizeof(buffer[0]), nread, out)){
					perror("fwrite()");
					RET(1);
				}
		}

		if(len){
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
