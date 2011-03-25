#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <alloca.h>

#include <unistd.h>

#include "http.h"
#include "file.h"
#include "ftp.h"

#include "output.h"
#include "util.h"

#include "wgetlite.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif


#define STR_EQUALS(a, b) !strcmp(a, b)

struct cfg global_cfg;
const char *argv0;


int proto_is_net(const char *proto)
{
	return STR_EQUALS(proto, "http") ||
		     STR_EQUALS(proto, "ftp");
}

char *proto_default_port(const char *proto)
{
	static char port_ftp[] = "21", port_http[] = "80";

	if(STR_EQUALS(proto, "ftp"))
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

int wget(const char *url)
{
	int sock, ret;

	FILE *f = NULL;
	size_t fpos = 0;
	char *outname;

	char *host, *file, *proto, *port;

	char *urlcpy = alloca(strlen(url) + 2);

	strcpy(urlcpy, url);

	if(!strchr(urlcpy, '/'))
		strcat(urlcpy, "/");

	if(parseurl(url, &host, &file, &proto, &port))
		return 1;

	if(global_cfg.out_fname){
		if(!strcmp(global_cfg.out_fname, "-")){
			outname = NULL;
		}else
			outname = strdup(global_cfg.out_fname);
	}else{
		char *the_last_slash_rated_pg_for_parental_guidance;

		the_last_slash_rated_pg_for_parental_guidance = strrchr(file, '/');
		if(the_last_slash_rated_pg_for_parental_guidance)
			outname = strdup(the_last_slash_rated_pg_for_parental_guidance + 1);
		else
			outname = strdup(file);

		/* TODO: urldecode outname (except for %3f) */
	}


	if(outname){
		if(strlen(outname) > PATH_MAX)
			outname[PATH_MAX - 1] = '\0';
		else if(!*outname){
			free(outname);
			outname = strdup("index.html");
		}

		/* if not explicitly specified, check for file overwrites */
		if(!global_cfg.out_fname && !access(outname, F_OK)){
			output_err(OUT_ERR, "%s: file exists", outname);
			goto bail;
		}

		f = fopen(outname, global_cfg.partial ? "a" : "w");
		if(!f){
			output_err(OUT_ERR, "open: \"%s\": %s", outname, strerror(errno));
			goto bail;
		}

		if(global_cfg.partial){
			long pos = ftell(f);
			if(pos == -1){
				output_perror("ftell()");
				goto bail;
			}
			fpos = pos;
		}
	}else
		f = stdout;


	if(proto_is_net(proto)){
		sock = dial(host, port);
		if(sock == -1){
			return 1;
		}
	}else
		sock = -1;

	/* TODO: function pointer */
	if(!strcmp(proto, "http"))
		ret = http_GET(sock, file, host, &f, fpos);
	else if(!strcmp(proto, "ftp"))
		ret = ftp_RETR(sock, file, host, &f, fpos);
	else if(!strcmp(proto, "file"))
		ret = file_copy(file, &f, fpos);
	else{
		ret = 1;
		output_err(OUT_ERR, "unknown protocol: %s", proto);
	}

fin:
	if(sock != -1)
		close(sock);

	if(f){
		fpos = ftell(f);

		if(f != stdout && fclose(f)){
			output_perror("close()");
			ret = 1;
		}
		if(!ret)
			output_err(OUT_INFO, "Saved to %s%s%s",
					outname ? "\"" : "",
					outname ? outname : "stdout",
					outname ? "\"" : "");
		else if(!fpos)
			/*
			 * Got a problem. So, if we wrote to the file,
			 * leave it be for a -c operation, otherwise unlink it
			 */
			remove(outname);
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

void sigh(int sig)
{
	fputs("we get signal!\n", stderr);
	exit(sig);
}

void verbosity_change(int dir)
{
#define v global_cfg.verbosity
	if(dir < 0 && v > 0)
		v--;
	else if(v < OUT_ERR)
		v++;
#undef v
}

int main(int argc, char **argv)
{
	int i;
	char *url = NULL;

	argv0 = *argv;

	memset(&global_cfg, 0, sizeof global_cfg);
	global_cfg.verbosity = OUT_INFO;

	setbuf(stdout, NULL);

	signal(SIGPIPE, sigh);

#define ARG(x) !strcmp(argv[i], "-" x)
#define verbosity_inc() verbosity_change(-1)
#define verbosity_dec() verbosity_change(+1)

	for(i = 1; i < argc; i++)
		if(     ARG("c")) global_cfg.partial = 1;
		else if(ARG("q")) verbosity_dec();
		else if(ARG("v")) verbosity_inc();

		else if(ARG("O")){
			if(!(global_cfg.out_fname = argv[++i]))
				goto usage;

		}else if(!url){
			url = argv[i];

		}else{
		usage:
			fprintf(stderr, "Usage: %s [-v] [-q] [-c] [-O file] url\n", *argv);
			return 1;
		}

	if(!url)
		goto usage;

	return wget(url);
}
