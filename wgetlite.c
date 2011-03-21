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

struct cfg global_cfg = { 0, NULL };

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

int parseurl(char *url,
		char **phost, char **pfile,
		char **pproto, char **pport)
{
#define host  (*phost)
#define file  (*pfile)
#define proto (*pproto)
#define port  (*pport)

	if((host = strstr(url, "://"))){
		proto = url;
		*host = '\0';
		host += 3;
	}else{
		host = url;
		proto = "http";
	}

	if(*host == '/'){
		file = host;
		host = NULL;
	}else{
		file = strchr(host, '/');
		if(!file){
			fprintf(stderr, "need file (try \"/\")\n");
			return 1;
		}

		if((port = strrchr(host, ':')))
			*port++ = '\0';
		else
			port = proto_default_port(proto);
		*file++ = '\0';
	}

	return 0;
#undef host
#undef file
#undef proto
#undef port
}

int wget(const char *url)
{
	FILE *f;
	char *host, *file, *proto, *sport;
	char *basename, *url_dup, *fullpath;
	int sock, ret;

	url_dup  = alloca(strlen(url) + 1);
	fullpath = alloca(strlen(url) + 1);
	strcpy(url_dup,  url);
	strcpy(fullpath, url);

	if(parseurl(url_dup, &host, &file, &proto, &sport))
		return 1;

	basename = strrchr(file, '/');
	if(!basename++)
		basename = file;

	if(!*basename)
		basename = "index.html";

#ifdef VERBOSE
	printf("host: %s\nproto: %s\nport: %s\nfile: \"%s\"\noutput: \"%s\"\n",
			host, proto, sport, file, basename);
#endif

	if(global_cfg.out_fname)
		basename = strdup(global_cfg.out_fname);
	else
		basename = strdup(basename);

	if(!basename){
		perror("strdup()");
		return 1;
	}

	if(strlen(basename) > PATH_MAX)
		basename[PATH_MAX - 1] = '\0';

	f = fopen(basename, "w");
	if(!f){
		fprintf(stderr, "open: \"%s\": %s\n", basename, strerror(errno));
		free(basename);
		return 1;
	}

	if(proto_is_net(proto)){
		sock = dial(host, sport);
		if(sock == -1){
			free(basename);
			return 1;
		}
	}else
		sock = -1;

	if(!strcmp(proto, "http"))
		ret = http_GET(sock, fullpath, &f);
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
		if(fclose(f)){
			perror("close()");
			ret = 1;
		}
		if(!ret)
			printf("saved to \"%s\"\n", basename);
	}

	free(basename);
	return ret;
}

void sigh(int sig)
{
	puts("we get signal!");
	exit(sig);
}

int main(int argc, char **argv)
{
	int i;
	char *url = NULL;

	setbuf(stdout, NULL);

	signal(SIGPIPE, sigh);

	for(i = 1; i < argc; i++)
		if(!strcmp(argv[i], "-v"))
			global_cfg.verbose = 1;
		else if(!strcmp(argv[i], "-O")){
			if(!(global_cfg.out_fname = argv[++i]))
				goto usage;
		}else if(!url)
			url = argv[i];
		else{
		usage:
			fprintf(stderr, "Usage: %s [-v] [-O file] url\n", *argv);
			return 1;
		}

	if(!url)
		goto usage;

	return wget(url);
}
