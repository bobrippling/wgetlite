#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "http.h"
#include "file.h"

int proto_is_net(const char *proto)
{
	return !strcmp(proto, "http");
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
			fprintf(stderr, "need file\n");
			return 1;
		}

		if((port = strrchr(host, ':')))
			*port++ = '\0';
		else
			port = "80";
		*file++ = '\0';
	}

	return 0;
#undef host
#undef file
#undef proto
#undef port
}

int dial(char *host, int port)
{
#define CHECK(f, s) \
	if(f){ \
		perror(s "()"); \
		return -1; \
	}
	struct sockaddr_in addr;
	struct hostent *hent;
	int sock;

	if(!(hent = gethostbyname(host))){
		fprintf(stderr, "gethostbyname(): %s\n", strerror(h_errno));
		return -1;
	}

	CHECK((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1, "socket");

	memset(&addr, 0, sizeof addr);
	memcpy(&addr.sin_addr.s_addr, hent->h_addr_list[0], sizeof addr.sin_addr.s_addr);
	addr.sin_port   = htons(port);
	addr.sin_family = AF_INET;

	CHECK(connect(sock, (struct sockaddr *)&addr, sizeof addr) == -1, "connect");

	return sock;
}

int wget(char *url)
{
	FILE *f;
	char *host, *file, *proto, *sport;
	char *basename, *url_dup;
	int sock, ret;

	url_dup = alloca(strlen(url) + 1);
	if(!url_dup){
		perror("malloc()");
		return 1;
	}
	strcpy(url_dup, url);

	if(parseurl(url, &host, &file, &proto, &sport))
		return 1;

	basename = strrchr(file, '/');
	if(!basename++)
		basename = file;

	if(!strlen(basename))
		basename = "index.html";

#ifdef VERBOSE
	printf("host: %s\nproto: %s\nport: %s\nfile: \"%s\"\noutput: \"%s\"\n",
			host, proto, sport, file, basename);
#endif

	f = fopen(basename, "w");
	if(!f){
		fprintf(stderr, "open: \"%s\": %s\n", basename, strerror(errno));
		return 1;
	}

	if(proto_is_net(proto)){
		sock = dial(host, atoi(sport));
		if(sock == -1)
			return 1;
	}else
		sock = -1;

	if(!strcmp(proto, "http"))
		ret = http_GET(sock, url_dup, f);
	else if(!strcmp(proto, "file"))
		ret = file_copy(file, f);
	else{
		ret = 1;
		fprintf(stderr, "unknown protocol: %s\n", proto);
	}

	if(sock != -1)
		close(sock);

	if(fclose(f)){
		perror("close()");
		ret = 1;
	}
	if(!ret)
		printf("saved to \"%s\"\n", basename);
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
		if(!url)
			url = argv[i];
		else{
		usage:
			fprintf(stderr, "Usage: %s url\n", *argv);
			return 1;
		}

	if(!url)
		goto usage;

	return wget(url);
}
