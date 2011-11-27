#include <stdio.h>
#include <stdarg.h>

#include "output.h"
#include "wgetlite.h"
#include "util.h"

void output_err(enum printlevel l, const char *fmt, ...)
{
	extern struct cfg global_cfg;
	extern const char *argv0;

	if(l >= global_cfg.verbosity){
		FILE *f = global_cfg.logf;
		va_list l;

		if(!f)
			f = stderr;

		fprintf(f, "\r%s: ", argv0);
		va_start(l, fmt);
		vfprintf(f, fmt, l);
		va_end(l);
		fputc('\n', f);
	}
}
