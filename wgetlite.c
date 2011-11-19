#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#ifndef __FreeBSD__
#include <alloca.h>
#endif

#include <unistd.h>

#include "output.h"
#include "wgetlite.h"

#include "http.h"
#include "ftp.h"
#include "gopher.h"

#include "util.h"
#include "term.h"

#include "connections.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif


#include "cookies.h"

char *proto_default_port(const char *proto)
{
	static char port_ftp[] = "21", port_http[] = "80",
		    port_gopher[] = "70";

	if(!strcmp(proto, "ftp"))
		return port_ftp;
	if(!strcmp(proto, "gopher"))
		return port_gopher;

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
		host  = xstrdup(host + 3);
		proto = xstrdup(url);
	}else{
		host = xstrdup(url);
		proto = xstrdup("http");
	}

	if(*host == '/'){
		/* file://tim.txt */
		file = host;
		host = NULL;
	}else{
		file = strchr(host, '/');

		if(file){
			char *slash = file;
			file = xstrdup(file);
			*slash = '\0';
		}else{
			file = xstrdup("/");
		}

		if((port = strrchr(host, ':')))
			*port++ = '\0';
		else
			port = proto_default_port(proto);
		port = xstrdup(port);
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
			finfo->outname = xstrdup("index.html");
		}

		/* if not explicitly specified, check for finfo->outname overwrites */
		if(!global_cfg.out_fname &&
				!global_cfg.partial &&
				!global_cfg.overwrite &&
				!access(finfo->outname, F_OK)){
			output_err(OUT_ERR, "%s: file exists", finfo->outname);
			return NULL;
		}

		ret = fopen(finfo->outname, mode ? mode : (global_cfg.partial ? "a+" : "w+"));
		if(!ret)
			output_err(OUT_ERR, "open: \"%s\": %s", finfo->outname, strerror(errno));

		return ret;
	}else{
		return stdout;
	}
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
	return wget_remove(finfo);
}

int wget_remove(struct wgetfile *finfo)
{
	return remove(finfo->outname);
}

int wget_connect(struct wgetfile *finfo)
{
	extern struct cfg global_cfg;
	const char *host, *port;

	if(finfo->proto == HTTP && global_cfg.http_proxy)
		host = global_cfg.http_proxy, port = global_cfg.http_proxy_port;
	else
		host = finfo->host_name, port = finfo->host_port;

	finfo->sock = connection_fd(host, port);

	if(finfo->sock == -1){
		finfo->sock = dial(host, port);
		if(finfo->sock == -1)
			return 1; /* dial() prints the error */
		connection_add(finfo->sock, host, port);
	}else{
		output_err(OUT_INFO, "Reusing connection to %s:%s", host, port);
	}
	return 0;
}

int wget(const char *url, int redirect_no)
{
	extern struct cfg global_cfg;
	struct wgetfile finfo;
	int ret;
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

	if(     !strcmp(proto, "http")) finfo.proto = HTTP;
	else if(!strcmp(proto, "ftp"))  finfo.proto = FTP;
	else if(!strcmp(proto, "gopher")) finfo.proto = GOPHER;
	else{
		ret = 1;
		output_err(OUT_ERR, "unknown protocol: %s", proto);
		goto bail;
	}

	free(proto);

	if(global_cfg.out_fname){
		if(!strcmp(global_cfg.out_fname, "-"))
			outname = NULL;
		else
			outname = xstrdup(global_cfg.out_fname);

		finfo.namemode = NAME_FORCE;
	}else{
		char *the_last_slash_rated_pg_for_parental_guidance;

		the_last_slash_rated_pg_for_parental_guidance = strrchr(file, '/');
		if(the_last_slash_rated_pg_for_parental_guidance)
			outname = xstrdup(the_last_slash_rated_pg_for_parental_guidance + 1);
		else
			outname = xstrdup(file);

		finfo.namemode = NAME_GUESS;

		/* TODO: urldecode outname (except for %3f) */
	}

	finfo.sock      = -1;
	finfo.host_file = file;
	finfo.host_name = host;
	finfo.host_port = port;
	finfo.outname   = outname;

	if(wget_connect(&finfo))
		goto bail;

	switch(finfo.proto) {
	default:
	case HTTP:
		ret = http_GET(&finfo);
		break;
	case FTP:
		ret = ftp_RETR(&finfo);
		break;
	case GOPHER:
		ret = gopher_retrieve(&finfo);
		break;
	}

fin:
	/* don't close the connection - let connection_* handle it */

	/* free the struct's members, since they might be changed */
	free(finfo.host_file);
	free(finfo.host_name);
	free(finfo.host_port);
	free(finfo.outname  );

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
