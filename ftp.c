#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>

#include "ftp.h"
#include "util.h"
#include "output.h"
#include "wgetlite.h"

#define WRITE(fd, str) write(fd, str, strlen(str))

#define FTP_FILE_STATUS        213
#define FTP_ENTER_PASSIVE_MODE 227
#define FTP_PROCEED            230
#define FTP_NEED_PASS          331
#define FTP_REQ_FILE_PENDING   350

int ftp_retcode(const char *s)
{
	int i;
	if(sscanf(s, "%d ", &i) == 1)
		return i;
	return -1;
}

int ftp_download(const char *host, const char *port, FILE *out,
		const char *fname, size_t len, size_t offset)
{
	int sock = dial(host, port);
	if(sock == -1)
		return 1;

	return generic_transfer(sock, out, fname, len, offset);
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
			else
				output_err(OUT_VERBOSE, "Server Info: \"%s\"", hyphen + 1);
		}while(1);
	}

	return line;
}

int ftp_RETR(int sock, const char *fname, FILE **out, size_t fpos)
{
	extern struct cfg global_cfg;
	char port[8], host[3 * 4 + 3 + 1] /* xxx.xxx.xxx.xxx */;
	char *line, *ident;
	size_t size;
	int i, h[4], p[2];

#define FTP_READLINE(b) if(!(line = ftp_readline(b))) return 1

	FTP_READLINE(sock);

	/* some sort of id... */
	ident = strchr(line, ' ');
	if(!*ident++)
		ident = line;

	output_err(OUT_INFO, "FTP Server Ident: %s", ident);
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
			output_err(OUT_INFO, "FTP Anonymous Login Success");
			break;


		default:
login_fail:
			free(line);
			output_err(OUT_ERR, "FTP Anonymous Login Failed");
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
		output_err(OUT_WARN, "FTP Warning - SIZE command failed (%s)");
		size = 0;
	}else if(sscanf(line, "%*d %zu", &size) != 1){
		output_err(OUT_WARN, "Couldn't parse SIZE result, ignoring");
		size = 0;
	}
	free(line);


	WRITE(sock, "PASV\r\n");
	FTP_READLINE(sock);
	i = ftp_retcode(line);
	if(i != FTP_ENTER_PASSIVE_MODE){
		output_err(OUT_ERR, "FTP PASV command failed (%d)", i);
		return 1;
	}

	/* parse the pasv reply for the address and ports */
	if(sscanf(line, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).",
				&h[0], &h[1], &h[2], &h[3], &p[0], &p[1]) != 6){
		free(line);
		output_err(OUT_ERR, "FTP PASV reply unparsable");
		return 1;
	}

	free(line);

	snprintf(port, sizeof port, "%d", (p[0] << 8) + p[1]);
	snprintf(host, sizeof host, "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);

	if(global_cfg.partial && fpos){
		char *reply;
		fdprintf(sock, "REST %ld\r\n", fpos); /* RESTart */
		FTP_READLINE(sock);

		i = ftp_retcode(line);
		reply = strchr(line, ' ');
		if(reply)
			output_err(OUT_INFO, "FTP REST: %s", reply + 1);
		free(line);

		if(i != FTP_REQ_FILE_PENDING){
			output_err(OUT_ERR, "FTP REST failed");
			return 1;
		}
	}

	fdprintf(sock, "RETR %s\r\n", fname);

	return ftp_download(host, port, *out, fname, size, fpos);
}
