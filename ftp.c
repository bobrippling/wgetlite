#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>

#include "util.h"

#define VERBOSE 1

#define WRITE(fd, str) write(fd, str, strlen(str))
#define READLINE_OR_FAIL(l, s) do { l = readline(s); if(!l){perror("readline()"); return 1;} }while(0)

int ftp_retcode(const char *s)
{
	int i;
	if(sscanf(s, "%d ", &i) == 1)
		return i;
	return -1;
}

int ftp_download(const char *host, int port, FILE *out, const char *fname, size_t len)
{
	int sock = dial(host, port);
	if(sock == -1)
		return 1;

	return generic_transfer(sock, out, fname, len);
}

int ftp_RETR(int sock, const char *fname, FILE *out)
{
	char *line, host[3 * 4 + 3 + 1]; /* xxx.xxx.xxx.xxx */
	size_t size;
	int i, h[4], p[2], port;

	READLINE_OR_FAIL(line, sock);

	/* some sort of id... */
#ifdef VERBOSE
	printf("FTP First line: %s\n", line);
#endif
	free(line);


	WRITE(sock, "USER anonymous\r\n");
	READLINE_OR_FAIL(line, sock);

	i = ftp_retcode(line);
	free(line);

	switch(i){
		case 331:
			WRITE(sock, "PASS -wgetlite@\r\n");
			READLINE_OR_FAIL(line, sock);

			i = ftp_retcode(line);
			free(line);

			if(i != 230)
				goto login_fail;


		case 230:
			/* login success */
#ifdef VERBOSE
			fputs("FTP Anonymous Login Success\n", stderr);
#endif
			break;


		default:
login_fail:
			free(line);
			fputs("FTP Anonymous Login Failed\n", stderr);
			return 1;
	}

	/* successful login if we reach here */


	WRITE(sock, "TYPE I\r\n");
	READLINE_OR_FAIL(line, sock);
	free(line); /* FIXME - check return code */

	fdprintf(sock, "SIZE %s", fname);
	READLINE_OR_FAIL(line, sock);
	i = ftp_retcode(line);
	if(i != 213){
		fprintf(stderr, "FTP SIZE command failed (%d)\n", i);
		free(line);
		return 1;
	}

	if(sscanf(line, "%*d %zu", &size) != 1){
		fputs("Couldn't parse SIZE result, ignoring\n", stderr);
		size = 0;
	}
	free(line);


	WRITE(sock, "PASV\r\n");
	READLINE_OR_FAIL(line, sock);
	i = ftp_retcode(line);
	if(i != 227){
		fprintf(stderr, "FTP PASV command failed (%d)\n", i);
		return 1;
	}

	/* parse the pasv reply for the address and ports */
	if(sscanf(line, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).",
				&h[0], &h[1], &h[2], &h[3], &p[0], &p[1]) != 6){
		free(line);
		fprintf(stderr, "FTP PASV reply unparsable\n");
		return 1;
	}

	free(line);

	port = (p[0] << 8) + p[1];
	sprintf(host, "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);

	fdprintf(sock, "RETR %s\r\n", fname);
	/* FIXME: check for any more messages? */
	return ftp_download(host, port, out, fname, size);
}
