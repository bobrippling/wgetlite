#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <alloca.h>

#include <unistd.h>

#include "output.h"
#include "wgetlite.h"

#include "http.h"
#include "ftp.h"

#include "util.h"
#include "term.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif


#include "cookies.h"

char *proto_default_port(const char *proto)
{
	static char port_ftp[] = "21", port_http[] = "80";

	if(!strcmp(proto, "ftp"))
		return port_ftp;

	return port_http;
}

/*
 * need protocol as well as port, since
 * we might be doing
 * wgetlite ftp://tim.com:80/path/to/file.txt
 */
int parseurl(const char *url,
		char **phost,  char **pfile,
		char **pproto, char **pport)
{
#define host  (*phost)
#define file  (*pfile)
#define proto (*pproto)
#define port  (*pport)

	if((host = strstr(url, "://"))){
		*host = '\0';
		host  = strdup(host + 3);
		proto = strdup(url);
	}else{
		host = strdup(url);
		proto = strdup("http");
	}

	if(*host == '/'){
		/* file://tim.txt */
		file = host;
		host = NULL;
	}else{
		file = strchr(host, '/');

		if(file){
			char *slash = file;
			file = strdup(file);
			*slash = '\0';
		}else{
			file = strdup("/");
		}

		if((port = strrchr(host, ':')))
			*port++ = '\0';
		else
			port = proto_default_port(proto);
		port = strdup(port);
	}

	return 0;
#undef host
#undef file
#undef proto
#undef port
}

FILE *wget_open(struct wgetfile *finfo, char *mode)
{
	if(finfo->outname){
		extern struct cfg global_cfg;
		FILE *ret;

		if(strlen(finfo->outname) > PATH_MAX)
			finfo->outname[PATH_MAX - 1] = '\0';
		else if(!*finfo->outname){
			free(finfo->outname);
			finfo->outname = strdup("index.html");
		}

		/* if not explicitly specified, check for finfo->outname overwrites */
		if(!global_cfg.out_fname &&
				!global_cfg.partial &&
				!access(finfo->outname, F_OK)){
			output_err(OUT_ERR, "%s: file exists", finfo->outname);
			return NULL;
		}

		ret = fopen(finfo->outname, mode ? mode : (global_cfg.partial ? "a" : "w"));
		if(!ret)
			output_err(OUT_ERR, "open: \"%s\": %s", finfo->outname, strerror(errno));

		return ret;
	}else
		return stdout;
}

int wget_close_verbose(struct wgetfile *finfo, FILE *f, int verbose)
{
	int ret = 0;

	if(f){
		long fpos = ftell(f);

		if(fpos == -1)
			fpos = 0;

		if(f != stdout && fclose(f)){
			if(verbose)
				output_perror("close()");
			ret = 1;
		}

		if(ret && !fpos)
			remove(finfo->outname);
			/*
			 * Got a problem and didn't write to the file,
			 * otherwise
			 * leave it be for a -c operation, otherwise unlink it
			 */
	}

	return ret;
}

int wget_close(struct wgetfile *finfo, FILE *f)
{
	return wget_close_verbose(finfo, f, 1);
}

int wget_remove_if_empty(struct wgetfile *finfo, FILE *f)
{
	if(ftell(f) > 0)
		return 0;
	wget_close(finfo, f);
	return wget_remove(finfo);
}

FILE *wget_close_and_open(struct wgetfile *finfo, FILE *f, char *mode)
{
	wget_close_verbose(finfo, f, 0);
	return wget_open(finfo, mode);
}

int wget_remove(struct wgetfile *finfo)
{
	return remove(finfo->outname);
}

int wget(const char *url, int redirect_no)
{
	extern struct cfg global_cfg;
	struct wgetfile finfo;
	wgetfunc *wgetfptr;
	int sock, ret;
	char *outname = NULL;
	char *host, *file, *proto, *port;

	char *urlcpy = alloca(strlen(url) + 2);

	strcpy(urlcpy, url);
	memset(&finfo, 0, sizeof finfo);
	finfo.redirect_no = redirect_no;

	if(!strchr(urlcpy, '/'))
		strcat(urlcpy, "/");

	if(parseurl(url, &host, &file, &proto, &port))
		return 1;

	if(     !strcmp(proto, "http")) wgetfptr = http_GET;
	else if(!strcmp(proto, "ftp"))  wgetfptr = ftp_RETR;
	/*else if(!strcmp(proto, "file")) wgetfptr = file_copy;*/
	else{
		/* FIXME: valgrind test */
		ret = 1;
		output_err(OUT_ERR, "unknown protocol: %s", proto);
		goto bail;
	}

	if(global_cfg.out_fname){
		if(!strcmp(global_cfg.out_fname, "-"))
			outname = NULL;
		else
			outname = strdup(global_cfg.out_fname);

		finfo.namemode = NAME_FORCE;
	}else{
		char *the_last_slash_rated_pg_for_parental_guidance;

		the_last_slash_rated_pg_for_parental_guidance = strrchr(file, '/');
		if(the_last_slash_rated_pg_for_parental_guidance)
			outname = strdup(the_last_slash_rated_pg_for_parental_guidance + 1);
		else
			outname = strdup(file);

		finfo.namemode = NAME_GUESS;

		/* TODO: urldecode outname (except for %3f) */
	}

	sock = dial(host, port);
	if(sock == -1)
		return 1; /* dial() prints the error */

	finfo.sock      = sock;
	finfo.host_file = file;
	finfo.host_name = host;
	finfo.host_port = port;
	finfo.outname   = outname;

	ret = wgetfptr(&finfo);

fin:
	if(finfo.sock != -1){
		close(finfo.sock);
		finfo.sock = -1;
	}

	free(outname);
	free(host);
	free(file);
	free(proto);
	free(port);

	return ret;
bail:
	ret = 1;
	goto fin;
}

void wget_success(struct wgetfile *finfo)
{
	output_err(OUT_INFO, "Saved '%s' -> '%s'",
			finfo->host_file,
			finfo->outname ? finfo->outname : "stdout");
}
