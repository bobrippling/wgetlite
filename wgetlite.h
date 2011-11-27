#ifndef WGETLITE_H
#define WGETLITE_H

struct cfg
{
	int partial, prog_dot, overwrite;
	enum printlevel verbosity;
	const char *out_fname;
	const char *user_agent;
	FILE *logf;

	const char *http_proxy, *http_proxy_port;
};

struct wgetfile
{
	enum { HTTP, FTP, GOPHER } proto;
	int sock;
	int redirect_no;
	char *host_file, *host_name, *host_port;
	char *outname;
	enum
	{
		/* order = precedence */
		NAME_FORCE, /* -O fname */
		NAME_AUTH,  /* HTTP headers tell us the name, etc */
		NAME_GUESS  /* $0 tim.com/hi.txt -> hi.txt */
	} namemode;
};

typedef int wgetfunc(struct wgetfile *);

int wget(const char *url, int redirect_no);
int wget_connect(struct wgetfile *);

FILE *wget_open( struct wgetfile *, char *mode);
int   wget_close(struct wgetfile *, FILE *);

int   wget_remove(struct wgetfile *);
int   wget_remove_if_empty(struct wgetfile *, FILE *);

void  wget_success(struct wgetfile *);
void  wget_failure(struct wgetfile *);

#endif
