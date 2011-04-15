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

#ifdef __GNUC__
# define GCC_PRINTF_ATTRIB(x, y) __attribute__((format(printf, x, y)))
#else
# define GCC_PRINTF_ATTRIB(x)
#endif

void output_err(enum printlevel, const char *, ...) GCC_PRINTF_ATTRIB(2, 3);
/*void output_perror(const char *);*/
#define output_perror(s) output_err(OUT_ERR, "%s: %s", s, strerror(errno))

#endif
