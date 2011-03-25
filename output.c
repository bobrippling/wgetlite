#include <stdio.h>
#include <stdarg.h>

#include "output.h"
#include "util.h"
#include "wgetlite.h"

void output_err(enum printlevel l, const char *fmt, ...)
{
	extern struct cfg global_cfg;
	extern const char *argv0;

	if(l >= global_cfg.verbosity){
		va_list l;
		fprintf(stderr, "%s: ", argv0);
		va_start(l, fmt);
		vfprintf(stderr, fmt, l);
		fputc('\n', stderr);
		va_end(l);
	}
}
