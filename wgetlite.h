#ifndef WGETLITE_H
#define WGETLITE_H

struct cfg
{
	int partial; /* TODO */
	enum printlevel verbosity;
	const char *out_fname;
};

int wget(const char *url);

#endif
