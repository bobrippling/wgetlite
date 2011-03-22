#ifndef WGETLITE_H
#define WGETLITE_H

struct cfg
{
	int verbose, partial, quiet;
	const char *out_fname;
};

int wget(const char *url);

#endif
