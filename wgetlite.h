#ifndef WGETLITE_H
#define WGETLITE_H

struct cfg
{
	int partial, prog_dot;
	enum printlevel verbosity;
	const char *out_fname;
};

struct wgetfile
{
	int sock;
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

int wget(const char *url);

FILE *wget_open( struct wgetfile *, char *mode);
FILE *wget_close_and_open(struct wgetfile *, FILE *, char *mode);

int   wget_close(struct wgetfile *, FILE *);
int   wget_close_if_empty(struct wgetfile *, FILE *);

int   wget_remove(struct wgetfile *);

void  wget_success(struct wgetfile *);

#endif
