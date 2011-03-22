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
#include "wgetlite.h"

#include "util.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif


#define STR_EQUALS(a, b) !strcmp(a, b)

struct cfg global_cfg;

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

		if(!file)
			file = strdup("/");
		else{
			char *slash = file;
			file = strdup(file);
			*slash = '\0';
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
	char *outname;

	char *host, *file, *proto, *port;

	char *urlcpy = alloca(strlen(url) + 1);

	strcpy(urlcpy, url);

	if(parseurl(url, &host, &file, &proto, &port))
		return 1;

	if(global_cfg.out_fname){
		if(!strcmp(global_cfg.out_fname, "-")){
			outname = NULL;
		}else
			outname = strdup(global_cfg.out_fname);
	}else
		outname = strdup(file);


	if(outname){
		if(strlen(outname) > PATH_MAX)
			outname[PATH_MAX - 1] = '\0';

		f = fopen(outname, "w");
		if(!f){
			fprintf(stderr, "open: \"%s\": %s\n", outname, strerror(errno));
			goto bail;
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

	if(!strcmp(proto, "http"))
		ret = http_GET(sock, urlcpy, &f);
	else if(!strcmp(proto, "ftp"))
		ret = ftp_RETR(sock, file, &f);
	else if(!strcmp(proto, "file"))
		ret = file_copy(file, &f);
	else{
		ret = 1;
		fprintf(stderr, "unknown protocol: %s\n", proto);
	}

	if(sock != -1)
		close(sock);

	if(f){
		if(f != stdout && fclose(f)){
			perror("close()");
			ret = 1;
		}
		if(!ret && !global_cfg.quiet)
			fprintf(stderr, "Saved to %s%s%s\n",
					outname ? "\"" : "",
					outname ? outname : "stdout",
					outname ? "\"" : "");
	}

	free(outname);
	return ret;
bail:
	/* TODO: free */
	return 1;
}

void sigh(int sig)
{
	fputs("we get signal!\n", stderr);
	exit(sig);
}

int main(int argc, char **argv)
{
	int i;
	char *url = NULL;

	memset(&global_cfg, 0, sizeof global_cfg);

	setbuf(stdout, NULL);

	signal(SIGPIPE, sigh);

#define ARG(x) !strcmp(argv[i], "-" x)

	for(i = 1; i < argc; i++)
		if(     ARG("c")) global_cfg.partial = 1;
		else if(ARG("q")) global_cfg.quiet   = 1;
		else if(ARG("v")) global_cfg.verbose = 1;

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
