#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>

#include "util.h"

#define WRITE(fd, str) write(fd, str, strlen(str))

#define FTP_FILE_STATUS        213
#define FTP_ENTER_PASSIVE_MODE 227
#define FTP_PROCEED            230
#define FTP_NEED_PASS          331

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

char *ftp_findhyphen(char *line)
{
	for(; *line; line++)
		if(*line == '-')
			return line;
		else if(!('0' <= *line && *line <= '9'))
			break;

	return NULL;
}

char *ftp_readline(int sock)
{
	char *line = readline(sock), *hyphen;

	if(!line)
		return NULL;

	if((hyphen = ftp_findhyphen(line))){
		/* extended response */
		do{
			free(line);
			line = readline(sock);
			if(!(hyphen = ftp_findhyphen(line)))
				break;
#ifdef VERBOSE
			else
				fprintf(stderr, "Server Info: \"%s\"\n", hyphen + 1);
#endif
		}while(1);
	}

	return line;
}

int ftp_RETR(int sock, const char *fname, FILE **out)
{
	char *line, host[3 * 4 + 3 + 1] /* xxx.xxx.xxx.xxx */, *ident;
	size_t size;
	int i, h[4], p[2], port;

#define FTP_READLINE(b) if(!(line = ftp_readline(b))) return 1

	FTP_READLINE(sock);

	/* some sort of id... */
	ident = strchr(line, ' ');
	if(!*ident++)
		ident = line;

	printf("FTP Server Ident: %s\n", ident);
	free(line);

	WRITE(sock, "USER anonymous\r\n");
	FTP_READLINE(sock);

	i = ftp_retcode(line);
	free(line);

	switch(i){
		case FTP_NEED_PASS:
			WRITE(sock, "PASS -wgetlite@\r\n");
			FTP_READLINE(sock);

			i = ftp_retcode(line);
			free(line);

			if(i != FTP_PROCEED)
				goto login_fail;


		case FTP_PROCEED:
			/* login success */
			fputs("FTP Anonymous Login Success\n", stderr);
			break;


		default:
login_fail:
			free(line);
			fputs("FTP Anonymous Login Failed\n", stderr);
			return 1;
	}

	/* successful login if we reach here */


	WRITE(sock, "TYPE I\r\n");
	FTP_READLINE(sock);
	free(line); /* FIXME - check return code */

	fdprintf(sock, "SIZE %s\r\n", fname);
	FTP_READLINE(sock);
	i = ftp_retcode(line);
	if(i != FTP_FILE_STATUS){
		fprintf(stderr, "FTP SIZE command failed (%s)\n", line);
		free(line);
		return 1;
	}

	if(sscanf(line, "%*d %zu", &size) != 1){
		fputs("Couldn't parse SIZE result, ignoring\n", stderr);
		size = 0;
	}
	free(line);


	WRITE(sock, "PASV\r\n");
	FTP_READLINE(sock);
	i = ftp_retcode(line);
	if(i != FTP_ENTER_PASSIVE_MODE){
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

	return ftp_download(host, port, *out, fname, size);
}
