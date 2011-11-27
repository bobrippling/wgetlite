#ifndef UTIL_H
#define UTIL_H

/* timing */
long mstime(void);


/* non-failing string functions */
char *xstrdup(const char *s);
char *xstrprintf(const char *, ...);

/* check if a string finishes with suffix */
const char *strfin(const char *s, const char *suffix);


/* socket util */
char *wget_readline(struct wgetfile *finfo);
int   wget_printf(struct wgetfile *finfo, const char *fmt, ...);
int   dial(const char *host, const char *port);

const char *bytes_to_str(long bits);

/* transfer however many bytes needed on a socket */
int generic_transfer(struct wgetfile *finfo, FILE *out, size_t len,
		size_t sofar, int closefd);

/* read len bytes from fd, checking for interrupts, etc */
int discard(struct wgetfile *finfo, int len);

#endif
