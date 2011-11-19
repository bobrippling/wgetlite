#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/time.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "progress.h"

#include "output.h"
#include "wgetlite.h"
#include "util.h"
#include "connections.h"

#define BSIZ 4096

static void alloc_die(unsigned int len)
{
	output_err(OUT_ERR, "Couldn't allocate %ud bytes", len);
	exit(1);
}

long mstime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

char *xstrdup(const char *s)
{
	unsigned int siz = strlen(s) + 1;
	char *d = malloc(siz);
	if(!d)
		alloc_die(siz);
	strcpy(d, s);
	return d;
}

char *wget_readline(struct wgetfile *finfo)
{
	static char buffer[BSIZ];
	char *pos;

#define RECV(len, rfunc, lbl_restart) \
lbl_restart: \
		switch(rfunc(finfo, buffer, len)){ \
			case -1: \
				if(errno == EINTR) \
					goto lbl_restart; \
				output_perror("recv()"); \
				return NULL; \
			case 0: /* unexpected here - need \r\n\r\n */ \
				return NULL; \
		}

	RECV(sizeof buffer, wget_peek, recv_1);

	if((pos = strchr(buffer, '\n'))){
		int len = pos - buffer + 1;
		RECV(len, wget_read, recv_2);

		if(pos > buffer && pos[-1] == '\r')
			pos[-1] = '\0';
		else
			*pos = '\0'; /* just \n, not \r\n, deal with it */

		return strdup(buffer);
	}

	output_err(OUT_ERR, "util::readline(): buffer too small");
	return NULL;
}

int wget_printf(struct wgetfile *finfo, const char *fmt, ...)
{
	char buffer[512];
	va_list l;
	unsigned int n;

	va_start(l, fmt);
	n = vsnprintf(buffer, sizeof buffer, fmt, l);
	va_end(l);

	if(n >= sizeof(buffer))
		output_err(OUT_ERR, "wget_printf() warning: buffer not large enough - FIXME");

	return wget_write(finfo, buffer, n);
}

int dial(const char *host, const char *port)
{
	struct addrinfo hints, *list = NULL, *iter;
	int sock, last_err;

	memset(&hints, '\0', sizeof hints);
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* TODO: printf("Resolving %s..."); ... printf(" %d.%d.%d.%d\n"); */

	if((last_err = getaddrinfo(host, port, &hints, &list))){
		output_err(OUT_ERR, "getaddrinfo(): \"%s:%s\": %s", host, port, gai_strerror(last_err));
		return -1;
	}


	/* TODO: printf("Connectan to ..."); "printf("... connected.\n"); */
	for(iter = list; iter; iter = iter->ai_next){
		sock = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);

		if(sock == -1)
			continue;

retry:
		if(connect(sock, iter->ai_addr, iter->ai_addrlen) == 0)
			break;
		else if(errno == EINTR)
			goto retry;

		if(errno)
			last_err = errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(list);

	if(sock == -1){
		errno = last_err;
		output_err(OUT_ERR, "connect to %s:%s: %s", host, port, strerror(errno));
	}
	return sock;
}

int generic_transfer(struct wgetfile *finfo, FILE *out, size_t len,
		size_t sofar, int closefd)
{
#define RET(n) do{ ret = n; goto fin; }while(0)
	int ret = 0;
	long last_progress, last_speed_calc;
	long chunk = 0; /* for bps */
	long speed = 0;

	last_progress = last_speed_calc = mstime();
	if(!len)
		progress_unknown(sofar, 0);

	do{
		ssize_t nread;
		char buffer[BSIZ];
		long t;

		switch((nread = wget_read(finfo, buffer, sizeof buffer))){
			case -1:
				if(errno == EINTR)
					continue;

				/* TODO: retry */
				if(len && sofar == len)
					goto end_of_stream;
				connection_close_fd(finfo->sock);
				/* should probably save errno from before */
				output_perror("recv()");
				RET(1);

			case 0:
end_of_stream:
				connection_close_fd(finfo->sock);

				if(len){
					if(sofar == len){
						RET(0);
					}else{
						/* TODO: goto retry */
						progress_incomplete();
						RET(1);
					}
				}else{
					/* no length, assume we have the whole file */
					RET(0);
				}
				/* unreachable */

			default:
			{
				int trunc = 0;

				if(len && sofar + nread > len){
					output_err(OUT_WARN, "too much data, truncating by %zu bytes", sofar + nread - len);
					trunc = 1;
					nread = len - sofar;
					sofar = len;
				}else{
					sofar += nread;
				}

				while(fwrite(buffer, sizeof(buffer[0]), nread, out) != (unsigned)nread){
					if(errno == EINTR)
						continue;
					output_perror("fwrite()");
					RET(1);
				}

				if(sofar == len)
					RET(0); /* don't wait for more, we may be http-pipelining */

				chunk += nread;

				if(trunc)
					goto end_of_stream;
			}
		}

		t = mstime();
		if(last_progress + 250 < t){
			/* spin the spinner, do it more frequently than we check for speed updates */

			if(last_speed_calc + 1000 < t){
				long tdiff = t - last_speed_calc;

				if(tdiff){
					speed = 1000.0f /* kbps */ * (float)chunk / (float)tdiff;
					chunk = 0;
					last_speed_calc = t;
				}
			}

			last_progress = t;
			if(len)
				progress_show(sofar, len, speed);
			else
				progress_unknown(sofar, speed);
		}
	}while(1);

fin:
	if(len)
		progress_fin(sofar, len);
	else
		progress_fin(0, 0);

	if(closefd) {
		ret |= wget_close(finfo, out);
		if(!ret)
			wget_success(finfo);
	}

	return ret;
}

const char *strfin(const char *s, const char *suffix)
{
	char *p = strchr(s, '\0');
	unsigned int l = strlen(suffix);

	if(l > strlen(s))
		return NULL;

	p -= l;
	return strcmp(p, suffix) ? NULL : p;
}

char *xstrprintf(const char *fmt, ...)
{
	va_list l;
	int n;
	char *buf;

	va_start(l, fmt);
	n = vsnprintf(NULL, 0, fmt, l);
	va_end(l);

	buf = malloc(n + 2);
	if(!buf)
		alloc_die(n + 2);

	va_start(l, fmt);
	vsnprintf(buf, n + 1, fmt, l);
	va_end(l);

	return buf;
}

int discard(struct wgetfile *finfo, int len)
{
	char bin[4096];

	do{
		int amt = sizeof bin;
		int n;

		if(amt > len)
			amt = len;

restart:
		n = wget_read(finfo, bin, amt);
		if(n == -1 && errno == EINTR)
			goto restart;
		if(n <= 0)
			return 1;

		len -= n;
	}while(len > 0);

	return 0;
}
