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
	char *host_file, *host_name;
	char *outname;
	long fpos;
};

typedef int wgetfunc(struct wgetfile *);

int wget(const char *url);

#endif
