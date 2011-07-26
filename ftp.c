#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>

#include "output.h"
#include "wgetlite.h"
#include "ftp.h"
#include "util.h"

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

int ftp_download(struct wgetfile *finfo, FILE *out,
		size_t len, size_t offset, char *host, char *port)
{
	const int oldsock = finfo->sock;
	int sock = dial(host, port);
	int ret;

	if(sock == -1)
		return 1;

	finfo->sock = sock;
	ret = generic_transfer(finfo, out, len, offset);
	close(oldsock);
	return ret;
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

	if(!line){
		output_err(OUT_ERR, "Premature end-of-stream");
		return NULL;
	}

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

int ftp_RETR(struct wgetfile *finfo)
{
	FILE *f;
	long fpos = 0;
	extern struct cfg global_cfg;
	char port[8], host[3 * 4 + 3 + 1] /* xxx.xxx.xxx.xxx */;
	char *line, *sptr;
	size_t size;
	int i, h[4], p[2];

#define FTP_READLINE() if(!(line = ftp_readline(finfo->sock))) return 1

	FTP_READLINE();

	/* some sort of id... */
	sptr = strchr(line, ' ');
	if(!*sptr++)
		sptr = line;

	output_err(OUT_INFO, "FTP Server Ident: %s", sptr);
	free(line);

	WRITE(finfo->sock, "USER anonymous\r\n");
	FTP_READLINE();

	i = ftp_retcode(line);
	free(line);

	switch(i){
		case FTP_NEED_PASS:
			WRITE(finfo->sock, "PASS -wgetlite@\r\n");
			FTP_READLINE();

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


	WRITE(finfo->sock, "TYPE I\r\n");
	FTP_READLINE();
	free(line); /* FIXME - check return code */

	fdprintf(finfo->sock, "SIZE %s\r\n", finfo->host_file);
	FTP_READLINE();
	i = ftp_retcode(line);
	if(i != FTP_FILE_STATUS){
		output_err(OUT_ERR, "FTP SIZE command failed (%s)", line);
		return 1;
	}else if(sscanf(line, "%*d %zu", &size) != 1){
		output_err(OUT_WARN, "Couldn't parse SIZE result, ignoring");
		size = 0;
	}
	free(line);


	WRITE(finfo->sock, "PASV\r\n");
	FTP_READLINE();
	i = ftp_retcode(line);
	if(i != FTP_ENTER_PASSIVE_MODE){
		output_err(OUT_ERR, "FTP PASV command failed (%d)", i);
		return 1;
	}

	/* parse the pasv reply for the address and ports */
	sptr = strrchr(line, '(');
	if(!sptr || sscanf(sptr, "(%d,%d,%d,%d,%d,%d)",
				&h[0], &h[1], &h[2], &h[3], &p[0], &p[1]) != 6){
		free(line);
		output_err(OUT_ERR, "FTP PASV reply unparsable (%s)", line);
		return 1;
	}

	free(line);

	snprintf(port, sizeof port, "%d", (p[0] << 8) + p[1]);
	snprintf(host, sizeof host, "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);

	f = wget_open(finfo, NULL);
	if(!f)
		return 1;

	if(global_cfg.partial && (fpos = ftell(f)) > 0){
		char *reply;
		fdprintf(finfo->sock, "REST %ld\r\n", fpos); /* RESTart */
		FTP_READLINE();

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

	fdprintf(finfo->sock, "RETR %s\r\n", finfo->host_file);

	return ftp_download(finfo, f, size, fpos, host, port);
}
