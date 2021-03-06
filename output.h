#ifndef OUTPUT_H
#define OUTPUT_H

enum printlevel
{
	OUT_DEBUG,
	OUT_VERBOSE,
	OUT_INFO,
	OUT_WARN,
	OUT_ERR
};

void output_err(enum printlevel, const char *, ...);

#define output_perror(s) output_err(OUT_ERR, "%s: %s", s, strerror(errno))

#endif
